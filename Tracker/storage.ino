#define STORAGE_FREE_CHAR 255

void storage_save_current() {
  DEBUG_FUNCTION_CALL();

  int data_len = strlen(data_current) + 1; // include '\0' as block marker
  //check for flash limit
  if (logindex + data_len >= STORAGE_DATA_END) {
    DEBUG_FUNCTION_PRINTLN("flash memory is full, starting to overwrite");
    // erase until the end of storage memory
    dueFlashStorage.write(logindex, (byte)STORAGE_FREE_CHAR, STORAGE_DATA_END - logindex);
    logindex = STORAGE_DATA_START;
  }
  DEBUG_PRINT("Saving current data: ");
  DEBUG_PRINTLN(data_current);

  //adding index marker, it will be overwritten with next flash write
  data_current[data_len] = STORAGE_FREE_CHAR;

  // detect overwrite
  if (dueFlashStorage.read(logindex + data_len) != STORAGE_FREE_CHAR) {
    // erase oldest data block
    byte *tmp = dueFlashStorage.readAddress(logindex + data_len);
    byte *tmpend = dueFlashStorage.readAddress(STORAGE_DATA_END);
    while (tmp < tmpend) {
      if (*tmp++ == 0) {
        DEBUG_PRINT("Erase until: ");
        DEBUG_PRINTLN(tmp - dueFlashStorage.readAddress(0));
        dueFlashStorage.write(logindex + data_len, (byte)STORAGE_FREE_CHAR, tmp - dueFlashStorage.readAddress(0) - logindex - data_len);
        break;
      }
    }
  }

  //saving data_current to flash memory, including index marker
  DEBUG_FUNCTION_PRINT("Saving at: ");
  DEBUG_PRINTLN(logindex);
  dueFlashStorage.write(logindex, (byte*)data_current, data_len + 1);
  if (memcmp(dueFlashStorage.readAddress(logindex), (byte*)data_current, data_len + 1) == 0) {
    logindex += data_len;

    DEBUG_PRINT("Index updated:");
    DEBUG_PRINTLN(logindex);
  }
  else
    DEBUG_FUNCTION_PRINTLN("verify failed");
  //storage_dump();
}

void storage_get_index() {
  //storage_dump();
  DEBUG_FUNCTION_CALL();
  DEBUG_PRINT("storage start: 0x");
  DEBUG_PRINTLN(FLASH_STORAGE_START, HEX);
  DEBUG_PRINT("storage size: ");
  DEBUG_PRINTLN(FLASH_STORAGE_SIZE);

  //scan flash for current log position (new log writes will continue from there)
  byte *tmp = dueFlashStorage.readAddress(STORAGE_DATA_START);
  byte *tmpend = dueFlashStorage.readAddress(STORAGE_DATA_END);
  bool block = false;
  while (tmp < tmpend) {
    if (!block && (*tmp != STORAGE_FREE_CHAR)) {
      block = true;
      DEBUG_PRINT("data found at: ");
      DEBUG_PRINTLN(tmp - dueFlashStorage.readAddress(0));
    }
    else if (block && (*tmp == STORAGE_FREE_CHAR)) {
      //found log index
      logindex = tmp - dueFlashStorage.readAddress(0);

      DEBUG_FUNCTION_PRINT("Found log position: ");
      break;  //we only need first found index
    }
    ++tmp;
  }
  if (tmp >= tmpend) { // probable corruption, re-initialize
    logindex = STORAGE_DATA_START;
    dueFlashStorage.write(logindex, STORAGE_FREE_CHAR);
    DEBUG_FUNCTION_PRINTLN("Not found, initialize log position: ");
  }
  DEBUG_PRINTLN(logindex);
}

// 1 = send stored data
// 0 = print data without sending
// -1 = test if any data stored
int storage_send_logs(int really_send) {
  int num_sent = 0;

  DEBUG_FUNCTION_CALL();

  //check if some logs were saved
  uint32_t sent_position = logindex;  //empty set

  byte *tmp = dueFlashStorage.readAddress(logindex);
  byte *tmpend = dueFlashStorage.readAddress(STORAGE_DATA_END);
  while (tmp < tmpend) {
    if (*tmp != STORAGE_FREE_CHAR) {
      //found sent position
      sent_position = tmp - dueFlashStorage.readAddress(0);

      DEBUG_FUNCTION_PRINT("Found post log sent position: ");
      break;  //we only need first found index
    }
    ++tmp;
  }
  if (tmp >= tmpend) { // re-start from the beginning
    tmp = dueFlashStorage.readAddress(STORAGE_DATA_START);
    tmpend = dueFlashStorage.readAddress(logindex);
    while (tmp < tmpend) {
      if (*tmp != STORAGE_FREE_CHAR) {
        //found sent position
        sent_position = tmp - dueFlashStorage.readAddress(0);

        DEBUG_FUNCTION_PRINT("Found pre log sent position: ");
        break;  //we only need first found index
      }
      ++tmp;
    }
  }
  DEBUG_PRINTLN(sent_position);

  if (really_send < 0) {
    return (sent_position != logindex);
  }

  if (sent_position != logindex) {
    bool err = false;
    long st = millis();
    do {
      DEBUG_FUNCTION_PRINT("Sending data from: ");
      DEBUG_PRINTLN(sent_position);

      // read current block
      strlcpy(data_current, (char*)dueFlashStorage.readAddress(sent_position), sizeof(data_current));
      int data_len = strlen(data_current) + 1; // include string terminator (zero)

      DEBUG_PRINT("Log data: ");
      DEBUG_PRINTLN(data_current);

      if (!really_send) {
        sent_position += data_len;
      } else {
        // send block
        if (gsm_send_data() == 1) {
          DEBUG_FUNCTION_PRINTLN("Success, erase sent data");

          // erase block (after sent)
          dueFlashStorage.write(sent_position, STORAGE_FREE_CHAR, data_len);
          sent_position += data_len;

          DEBUG_PRINT("Advance sent data:");
          DEBUG_PRINTLN(sent_position);

          // apply send limit
          num_sent++;
          if ((STORAGE_MAX_SEND_OLD > 0 && num_sent >= STORAGE_MAX_SEND_OLD) ||
              (STORAGE_MAX_SEND_TIME > 0 && (long)(millis() - st) > STORAGE_MAX_SEND_TIME)) {
            DEBUG_FUNCTION_PRINTLN("Reached send limit");
            break;
          }
        } else {
          err = true;
          break;
        }
      }
      if (sent_position >= STORAGE_DATA_END || (sent_position > logindex && dueFlashStorage.read(sent_position) == STORAGE_FREE_CHAR)) {
        DEBUG_FUNCTION_PRINTLN("Wrap around sending data");
        sent_position = STORAGE_DATA_START;
      }
    } while (sent_position != logindex && dueFlashStorage.read(sent_position) != STORAGE_FREE_CHAR);

    if (!err) {
      DEBUG_FUNCTION_PRINTLN("Logs were sent successfully");
    } else {
      DEBUG_FUNCTION_PRINTLN("Error sending logs");
    }
  } else {
    DEBUG_FUNCTION_PRINTLN("No logs present");
  }

  return num_sent;
}

void storage_dump() {
  DEBUG_FUNCTION_CALL();
  DEBUG_PRINT("start = ");
  DEBUG_PRINTLN(STORAGE_DATA_START);
  DEBUG_PRINT("end = ");
  DEBUG_PRINTLN(STORAGE_DATA_END);
  DEBUG_PRINT("logindex = ");
  DEBUG_PRINTLN(logindex);
  byte *tmp = dueFlashStorage.readAddress(STORAGE_DATA_START);
  byte *tmpend = dueFlashStorage.readAddress(STORAGE_DATA_END);
  int k = 0;
  char buf[10];
  while (tmp < tmpend) {
    if ((k & 31) == 0)
      DEBUG_PRINTLN();
    snprintf(buf, 10, "%02X", *tmp);
    DEBUG_PRINT(buf);
    ++k;
    ++tmp;
  }
  DEBUG_PRINTLN();
}
