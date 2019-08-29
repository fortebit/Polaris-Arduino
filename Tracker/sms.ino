void sms_check() {
  sms_check(30);
}

//check SMS
void sms_check(int max_count) {
  int msg_count = 30; // default
  char *tmp = NULL, *tmpcmd = NULL;
  char phone[32] = "";
  char msg[160];

  DEBUG_FUNCTION_CALL();

  if (max_count >= 0) {
  gsm_get_reply(1); // clear buffer
  gsm_port.print("AT+CPMS?\r");
  gsm_wait_for_reply(1,1);

  tmp = strstr(modem_reply, "+CPMS: ");
  if(tmp!=NULL) {
    tmp = strtok(tmp + 7, ",\"");
    if(tmp!=NULL) {
      tmp = strtok(NULL, ",\"");
      if(tmp!=NULL) {
        tmp = strtok(NULL, ",\"");
        if(tmp!=NULL) {
          msg_count = atoi(tmp);
          DEBUG_PRINT("SMS storage total: ");
          DEBUG_PRINTLN(msg_count);
        }
      }
    }
  }
    if (max_count > 0 && msg_count > max_count)
      msg_count = max_count;
  } else {
    msg_count = -max_count;
  }

  for(int i=1;i<=msg_count;i++) {
    gsm_get_reply(1); // clear buffer

    gsm_port.print("AT+CMGR=");
    gsm_port.print(i);
    gsm_port.print("\r");
  
    gsm_wait_for_reply(1,1);
  
    //phone info will look like this: +CMGR: "REC READ","+436601601234","","5 12:13:17+04"
    //phone will start from ","+  and end with ",
    tmp = strstr(modem_reply, "+CMGR:");
    if(tmp!=NULL) {
      tmp = strstr(modem_reply, "READ\",\"");
      if(tmp!=NULL) {
        // find start of commands
        tmpcmd = strstr(modem_reply, "\r\n#");
        
        // get sender phone number
        tmp += 7;
        tmp = strtok(tmp, "\",\"");
        if(tmp!=NULL) {
          strlcpy(phone, tmp, sizeof(phone));
          DEBUG_PRINT("Phone: ");
          DEBUG_PRINTLN(phone);
        }

        // find end of commands
        tmp = strstr(tmpcmd, "\r\nOK\r\n");
        if(tmpcmd!=NULL && tmp!=NULL) {
          // make a local copy (since this is coming from modem_reply, it will be overwritten)
          *tmp = '\0';
          strlcpy(msg, tmpcmd + 3, sizeof(msg));
          
          //next data is probably command till \r
          //all data before "," is sms password, the rest is command
          DEBUG_PRINT("SMS command found: ");
          DEBUG_PRINTLN(msg);

          sms_cmd(msg, phone);
        }
      }
      
      DEBUG_PRINT("Delete message");
    
      gsm_get_reply(1); // clear buffer
  
      gsm_port.print("AT+CMGD=");
      gsm_port.print(i);
      gsm_port.print("\r");
    
      gsm_wait_for_reply(1,0);
    }

    status_delay(5);
  }
}

void sms_cmd(char *msg, char *phone) {
  char *tmp;
  int i=0;

  DEBUG_FUNCTION_CALL();

  //command separated by "," format: password,command=value
  tmp = strtok(msg, ",");
  while (tmp != NULL && i < 10) {
    if(i == 0) {
      bool auth = true;
#if SMS_CHECK_INCLUDE_IMEI
      //check IMEI
      tmp1 = strtok(tmp, "@");
      if(tmp1 != NULL)
        tmp1 = strtok(NULL, ",");
      if(tmp1 == NULL || strcmp(tmp1, config.imei) != 0)
        auth = false;
      else
#endif
      //checking password
      if(strcmp(tmp, config.sms_key) != 0)
        auth = false;
      if(auth) {
        DEBUG_FUNCTION_PRINT("SMS password accepted, executing commands from ");
        DEBUG_PRINTLN(phone);
      } else {
        DEBUG_FUNCTION_PRINTLN("SMS password failed, ignoring commands");
        break;
      }
    } else {
      sms_cmd_run(tmp, phone);
    }
    tmp = strtok(NULL, ",\r\n");
    i++;
  }
}

void sms_cmd_run(char *cmd, char *phone) {
  char *tmp = NULL;
  char *tmpcmd = NULL;
  long val;

  char msg[160];

  DEBUG_FUNCTION_CALL();

  //checking what command to execute
  cmd = strtok_r(cmd, "=", &tmpcmd);
  if(cmd != NULL) {
    //get argument (if any)
    tmp = strtok_r(NULL, ",\r", &tmpcmd);
  }
  DEBUG_PRINTLN(cmd);
  DEBUG_PRINTLN(tmp);
  
  //set APN
  if(strcmp(cmd,"apn") == 0) {
    //setting new APN
    DEBUG_FUNCTION_PRINT("New APN=");
    DEBUG_PRINTLN(tmp);

    //updating APN in config
    strlcpy(config.apn, tmp, sizeof(config.apn));

    DEBUG_PRINT("New APN configured: ");
    DEBUG_PRINTLN(config.apn);

    save_config=1;
    power_reboot=1;

    //send SMS reply
    sms_send_msg("APN saved", phone);
  }
  else
  //APN password
  if(strcmp(cmd, "gprspass") == 0) {
    //setting new APN pass
    DEBUG_FUNCTION_PRINT("New APN pass=");
    DEBUG_PRINTLN(tmp);

    //updating in config
    strlcpy(config.pwd, tmp, sizeof(config.pwd));

    DEBUG_PRINT("New APN pass configured: ");
    DEBUG_PRINTLN(config.pwd);

    save_config=1;
    power_reboot=1;

    //send SMS reply
    sms_send_msg("APN password saved", phone);
  }
  else
  //APN username
  if(strcmp(cmd, "gprsuser") == 0) {
    //setting new APN
    DEBUG_FUNCTION_PRINT("New APN user=");
    DEBUG_PRINTLN(tmp);

    //updating APN in config
    strlcpy(config.user, tmp, sizeof(config.user));

    DEBUG_PRINT("New APN user configured: ");
    DEBUG_PRINTLN(config.user);

    save_config=1;
    power_reboot=1;

    //send SMS reply
    sms_send_msg("APN username saved", phone);
  }
  else
  //SMS pass
  if(strcmp(cmd, "smspass") == 0) {
    //setting new APN
    DEBUG_FUNCTION_PRINT("New smspass=");
    DEBUG_PRINTLN(tmp);

    //updating APN in config
    strlcpy(config.sms_key, tmp, sizeof(config.sms_key));

    DEBUG_PRINT("New sms_key configured: ");
    DEBUG_PRINTLN(config.sms_key);

    save_config=1;

    //send SMS reply
    sms_send_msg("SMS password saved", phone);
  }
  else
  //PIN
  if(strcmp(cmd, "pin") == 0) {
    //setting new APN
    DEBUG_FUNCTION_PRINT("New pin=");
    DEBUG_PRINTLN(tmp);

    //updating pin in config
    strlcpy(config.sim_pin, tmp, sizeof(config.sim_pin));

    DEBUG_PRINT("New sim_pin configured: ");
    DEBUG_PRINTLN(config.sim_pin);

    save_config=1;
    power_reboot=1;

    //send SMS reply
    sms_send_msg("SIM pin saved", phone);
  }
  else
  //alarm
  if(strcmp(cmd, "alarm") == 0) {
    //setting alarm
    DEBUG_FUNCTION_PRINT("New Alarm=");
    DEBUG_PRINTLN(tmp);
    if(strcmp(tmp, "off") == 0) {
      config.alarm_on = 0;
    } else if(strcmp(tmp, "on") == 0) {
      config.alarm_on = 1;
    }
    //updating alarm phone
    strlcpy(config.alarm_phone, phone, sizeof(config.alarm_phone));
    DEBUG_PRINT("New alarm_phone configured: ");
    DEBUG_PRINTLN(config.alarm_phone);
    save_config = 1;
    //send SMS reply
    if(config.alarm_on) {
      sms_send_msg("Alarm set ON", phone);
    } else {
      sms_send_msg("Alarm set OFF", phone);
    }
  }
  else
  //interval
  if(strcmp(cmd, "int") == 0) {
    //setting new APN
    DEBUG_FUNCTION_PRINT("New interval=");
    DEBUG_PRINTLN(tmp);

    val = atol(tmp);

    if(val > 0) {
      //updating interval in config (convert to milliseconds)
      config.interval = val*1000;

      DEBUG_PRINT("New interval configured: ");
      DEBUG_PRINTLN(config.interval);

      save_config=1;

      //send SMS reply
      sms_send_msg("Interval saved", phone);
    }
    else
      DEBUG_FUNCTION_PRINT("invalid value");
  }
  else
  if(strcmp(cmd, "locate") == 0) {
    DEBUG_FUNCTION_PRINTLN("Locate command detected");

    if(LOCATE_COMMAND_FORMAT_IOS) {
      snprintf(msg,sizeof(msg),"comgooglemaps://?center=%s,%s",lat_current,lon_current);
    } else {
      snprintf(msg,sizeof(msg),"https://maps.google.com/maps/place/%s,%s",lat_current,lon_current);
    }
    sms_send_msg(msg, phone);
  }
  else
  if(strcmp(cmd, "tomtom") == 0) {
    DEBUG_FUNCTION_PRINTLN("TomTom command detected");

    snprintf(msg,sizeof(msg),"tomtomhome://geo:lat=%s&long=%s",lat_current,lon_current);
    sms_send_msg(msg, phone);
  }
  else
  if(strcmp(cmd, "data") == 0) {
    DEBUG_FUNCTION_PRINT("New Data=");
    DEBUG_PRINTLN(tmp);
    if(strcmp(tmp, "off") == 0) {
      SEND_DATA = 0;
    } else if(strcmp(tmp, "on") == 0) {
      SEND_DATA = 1;
    }
    //send SMS reply
    if(SEND_DATA) {
      sms_send_msg("Data ON", phone);
    } else {
      sms_send_msg("Data OFF", phone);
    }
  }
  else
  if(strcmp(cmd, "getimei") == 0) {
    DEBUG_FUNCTION_PRINTLN("Get IMEI command detected");
    snprintf(msg,sizeof(msg),"IMEI: %s",config.imei);
    sms_send_msg(msg, phone);
  }
#if DEBUG
  else
  if(strcmp(cmd, "debug") == 0) {
    DEBUG_FUNCTION_PRINT("New Debug=");
    DEBUG_PRINTLN(tmp);
    if(strcmp(tmp, "off") == 0) {
      config.debug = 0;
      save_config = 1;
    } else if(strcmp(tmp, "on") == 0) {
      config.debug = 1;
      save_config = 1;
    }
    else
      DEBUG_FUNCTION_PRINTLN("invalid value");
    //send SMS reply
    if (config.debug == 1) {
      usb_console_restore();
      sms_send_msg("Debug ON", phone);
    } else {
      sms_send_msg("Debug OFF", phone);
      usb_console_disable();
    }
  }
#endif
  else
  if(strcmp(cmd, "powersave") == 0) {
    DEBUG_FUNCTION_PRINT("New Powersave=");
    DEBUG_PRINTLN(tmp);
    if(strcmp(tmp, "off") == 0) {
      config.powersave = 0;
      save_config = 1;
    } else if(strcmp(tmp, "on") == 0) {
      config.powersave = 1;
      save_config = 1;
    }
    else
      DEBUG_FUNCTION_PRINTLN("invalid value");
    //send SMS reply
    if (config.powersave == 1) {
      sms_send_msg("Powersave ON", phone);
    } else {
      sms_send_msg("Powersave OFF", phone);
    }
  }
  else
    addon_sms_command(cmd, tmp, phone);
}

void sms_send_msg(const char *cmd, const char *phone) {
  //send SMS message to number
  DEBUG_FUNCTION_CALL();

  gsm_get_reply(1); // clear buffer

  DEBUG_PRINT("Sending SMS to:");
  DEBUG_PRINTLN(phone);
  DEBUG_PRINTLN(cmd);

  gsm_port.print("AT+CMGS=\"");
  gsm_port.print(phone);
  gsm_port.print("\"\r");

  gsm_wait_for_reply(0,0);

  const char *tmp = strstr(modem_reply, ">");
  if(tmp!=NULL) {
    DEBUG_PRINT("Modem replied with >");
    gsm_port.print(cmd);

    //sending ctrl+z
    gsm_port.print("\x1A");

    gsm_wait_for_reply(1,0);
  }
}
