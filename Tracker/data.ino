#include <time.h>
#include <stdarg.h>

//collect and send GPS data for sending
#define MAX_DATA_INDEX (int)(sizeof(data_current) - 2)

void data_append_char(char c) {
  if (data_index < MAX_DATA_INDEX) {
    data_current[data_index++] = c;
    data_current[data_index] = 0;
  }
}

void data_append_string(const char *str) {
  while (*str != 0 && data_index < MAX_DATA_INDEX)
    data_current[data_index++] = *str++;
  data_current[data_index] = 0;
}

void data_append_legacy_sensor(const char *str) {
#ifdef HTTP_USE_JSON
  data_append_char('\"');
  data_append_string(str);
  data_append_char('\"');
  data_append_char(':');
#endif
}

void data_append_sensor(const char *str) {
#ifdef HTTP_USE_JSON
  data_append_char('\"');
  data_append_string(str);
  data_append_char('\"');
  data_append_char(':');
#else // use old format
  data_append_string(str);
  data_append_char(':');
#endif
}

void data_append_format(const char *fmt, ...) {
  va_list vl;
  va_start(vl, fmt);
  vsnprintf(data_current + data_index, MAX_DATA_INDEX - data_index + 1, fmt, vl);
  va_end(vl);
}

void data_reset() {
  // make sure there is always a string terminator
  memset(data_current, 0, sizeof(data_current));
  data_index = 0;
}

bool data_sep_flag = false;

void data_field_separator(char c) {
  if (data_sep_flag)
    data_append_char(c);
  data_sep_flag = true;
}

void data_field_restart() {
  data_sep_flag = false;
}

// url encoding functions

char to_hex(int nibble) {
  static const char hex[] = "0123456789abcdef";
  return hex[nibble & 15];
}

bool is_url_safe(char c) {
  if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
    || c == '-' || c == '_' || c == '.' || c == '~' || c == '!' || c == '*' || c == '\'' || c == '(' || c == ')')
    return true;
  return false;
}

int url_encoded_strlen(const char* s) {
  int len = strlen(s);
  int ret = 0;
  while (len--) {
    ret += is_url_safe(*s++) ? 1 : 3;
  }
  return ret;
}

// return count of consumed source characters (that fit the buffer after encoding)
int url_encoded_strlcpy(char* dst, int maxlen, const char* src) {
  int len = strlen(src);
  int count = 0;
  while (len > 0 && maxlen > 4) {
    char c = *src++;
    ++count;
    --len;
    if (is_url_safe(c)) {
      *dst++ = c;
      --maxlen;
    } else {
      *dst++ = '%';
      *dst++ = to_hex(c >> 4);
      *dst++ = to_hex(c & 15);
      maxlen -= 3;
    }
  }
  *dst = '\0';
  return count;
}

// read and convert analog input voltage
float analog_input_voltage(int pin, int range)
{
  float sensorValue = analogRead(pin);
  if (range == HIGH)
    sensorValue *= ANALOG_SCALE * ANALOG_VREF / 1024.0f;
  if (range == LOW)
    sensorValue *= ANALOG_SCALE_LOW * ANALOG_VREF / 1024.0f;
  return sensorValue;
}

/**
* This is default collect data function for HTTP
*/
void collect_all_data(int ignitionState) {
  DEBUG_FUNCTION_CALL();

  data_field_restart();
  
  //get current time and add to this data packet
  gsm_get_time();

  //convert to UTC (strip time zone info)
  time_char[17] = 0;

#ifdef HTTP_USE_JSON
  //indicate start of JSON
  data_append_char('{');
#ifdef HTTP_USE_JSON_TIMESTAMP
  data_append_string("\"values\":{");
#endif
#else
  //attach time to data packet
  data_append_string(time_char);
  
  //indicate start of GPS data packet
  data_append_char('[');
#endif
  data_field_restart();

  collect_gps_data();
  
#ifndef HTTP_USE_JSON
  //indicate stop of GPS data packet
  data_append_char(']');

  data_field_restart();
#endif

  // append main battery level to data packet
  if(DATA_INCLUDE_BATTERY_LEVEL) {
    data_field_separator(',');
    data_append_legacy_sensor("battery");
    float sensorValue = analog_input_voltage(AIN_S_INLEVEL, HIGH);
    data_append_float(sensorValue,2,2);
  }

#ifdef AIN_BATT_VOLT
  // append backup battery level to data packet
  if(DATA_INCLUDE_BACKUP_LEVEL) {
    data_field_separator(',');
    data_append_legacy_sensor("backup");
    float sensorValue = analog_input_voltage(AIN_BATT_VOLT, LOW);
    data_append_float(sensorValue,2,2);
  }

  // append charger status to data packet
  if(DATA_INCLUDE_CHARGER_STATUS) {
    data_field_separator(',');
    data_append_legacy_sensor("charger");

    int charge = -1;
    if (battery_get_source() == 0)
      charge = battery_get_status();
    data_append_long(charge);
  }
#endif

  // ignition state
  if(DATA_INCLUDE_IGNITION_STATE) {
    data_field_separator(',');
    data_append_legacy_sensor("ignition");

    if(ignitionState == -1) {
      data_append_char('2'); // backup source
    } else if(ignitionState == 0) {
      data_append_char('1');
    } else {
      data_append_char('0');
    }
  }

  // engine running time
  if(DATA_INCLUDE_ENGINE_RUNNING_TIME) {
    unsigned long currentRunningTime = engineRunningTime;
    if(engineRunning == 0) {
      currentRunningTime += (millis() - engine_start);
    }

    data_field_separator(',');
    data_append_legacy_sensor("running");
    data_append_long(currentRunningTime / 1000);
  }

  addon_collect_data();

#ifdef HTTP_USE_JSON
#ifdef HTTP_USE_JSON_TIMESTAMP
  struct tm tm;
  time_t epoch;
  if ( sscanf(time_char, "%02d/%02d/%02d,%02d:%02d:%02d",
      &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
      &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6 ) {
    tm.tm_year += 2000 - 1900; // offset since 1900
    tm.tm_mon -= 1; // zero based
    tm.tm_yday = 0;
    tm.tm_wday = 0;
    tm.tm_isdst = 0;
    epoch = mktime(&tm); // seconds
    char tmp[20];
    ltoa(epoch, tmp, 10);
    data_append_string("},\"ts\":");
    data_append_string(tmp);
    data_append_string("000"); // convert to milliseconds
  }
#endif
  // end of JSON
  data_append_char('}');
#else
  //end of data packet
  data_append_char('\n');
#endif
}

/**
 * This function send collected data using HTTP or TCP
 */
void send_data() {
  DEBUG_FUNCTION_CALL();
  
  DEBUG_PRINT("Current:");
  DEBUG_PRINTLN(data_current);

  interval_count++;
  DEBUG_PRINT("Data accumulated:");
  DEBUG_PRINTLN(interval_count);
  
  // send accumulated data
  if (interval_count >= config.interval_send) {
    // if data send disabled, use storage
    if (!SEND_DATA) {
      DEBUG_PRINT("Data send is turned off.");
#if STORAGE
      storage_save_current();   //in case this fails - data is lost
#endif
    } else if (gsm_send_data() != 1) {
      DEBUG_PRINT("Could not send data.");
#if STORAGE
      storage_save_current();   //in case this fails - data is lost
#endif
    } else {
      DEBUG_PRINT("Data sent successfully.");
#if STORAGE
      //connection seems ok, send saved data history
      storage_send_logs(1); // 0 = dump only, 1 = send data
#endif
    }
    
    //reset current data and counter
    data_reset();
    interval_count -= config.interval_send;
  }
}
