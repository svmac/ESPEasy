//#######################################################################################################
//#################################### Plugin 135: RollerShutter ########################################
//#######################################################################################################
// written by Salva Vargas

// List of commands:
// (0) <name>.STOP                                  Stop current action
// (1) <name>.OPEN, <name>.UP or <name>.UP,0        Open RollerShutter
// (2) <name>.CLOSE, <name>.DOWN or <name>.DOWN,0   Close RollerShutter
// (3) <name>.SET,<pos 0-100>                       Set position (0-100, 0=Top, 99=Bottom, 100=Closed)
// (4) <name>.UP,<pos 1-100>                        If actual position > pos then Set position
// (5) <name>.DOWN,<pos 1-100>                      If actual position < pos then Set position


//#ifdef USES_P135

#define PLUGIN_135
#define PLUGIN_ID_135         135
#define PLUGIN_NAME_135       "Output - RollerShutter"
#define PLUGIN_VALUENAME1_135 "Position"  //0-100, 0=Open, 99=Down (Botton), 100=Closed
#define PLUGIN_VALUENAME2_135 "Status"		//0=Uknown (not initialized), 1=Stop, 2=Up, 3=Down
#define PLUGIN_VALUENAME3_135 "RealPos"   //0-100-1xx, 0=Open, 100=Down, 1xx=Closed
#define PLUGIN_VALUENAME4_135 "FinalPos"  //0-100-1xx, 0=Open, 100=Down, 1xx=Closed


#define P135_TTH (Settings.TaskDevicePluginConfigLong[event->TaskIndex][1])   // Time (ms) from Top to Half
#define P135_TTB (Settings.TaskDevicePluginConfigLong[event->TaskIndex][2])   // Time (ms) from Top to Botton
#define P135_TTC (Settings.TaskDevicePluginConfigLong[event->TaskIndex][0])   // Time (ms) from Top to Closed
#define P135_TCT (Settings.TaskDevicePluginConfigFloat[event->TaskIndex][0])   // Time (ms) from Closed to Top
#define P135_TCB (Settings.TaskDevicePluginConfigFloat[event->TaskIndex][2])  // Time (ms) from Closed to Botton
#define P135_TCH (Settings.TaskDevicePluginConfigFloat[event->TaskIndex][1])  // Time (ms) from Closed to Half

#define P135_PIN1 (Settings.TaskDevicePin1[event->TaskIndex])
#define P135_PIN2 (Settings.TaskDevicePin2[event->TaskIndex])
#define P135_LOWACTIVE (Settings.TaskDevicePin1Inversed[event->TaskIndex])    // GPIO Low Active
#define P135_GMODE (Settings.TaskDevicePluginConfig[event->TaskIndex][0])     // GPIO Mode

#define P135_POS (UserVar[event->BaseVarIndex])                               // Position
#define P135_STAT (Settings.TaskDevicePluginConfig[event->TaskIndex][1])      // Internal Status
#define P135_XSTAT (UserVar[event->BaseVarIndex + 1])                         // External Status
#define P135_RPOS (UserVar[event->BaseVarIndex + 2])                          // Real Position
#define P135_FPOS (UserVar[event->BaseVarIndex + 3])                          // Final Position

#define P135_MILLIS (Settings.TaskDevicePluginConfigLong[event->TaskIndex][3])  // Millis timestamp
#define P135_MSPOS (Settings.TaskDevicePluginConfigFloat[event->TaskIndex][3])  // Time position  
#define P135_PREVPOS (Settings.TaskDevicePluginConfig[event->TaskIndex][2])     // Previus position
#define P135_PAC (Settings.TaskDevicePluginConfig[event->TaskIndex][3])
#define P135_MAXPOSUD (Plugin_135_getRPos(P135_TTC,P135_TTH,P135_TTB))                    // Max position up/down
#define P135_MAXPOSDU (Plugin_135_getRPos(P135_TCT,P135_TCT-P135_TCH,P135_TCT-P135_TCB))  // Max position down/up


// STATUSES
#define P135_UKNOWN     0     // Position uknown
#define P135_STOP       1     // Position nown, engine stop
#define P135_UP         2     // Position nown, engine up
#define P135_DOWN       3     // Position nown, engine down
#define P135_UK_STOP    11    // Position uknown, engine stop
#define P135_UK_UP      12    // Position uknown, engine up
#define P135_UK_DOWN    13    // Position uknown, engine down


#define P135_VTYPE SENSOR_TYPE_DUAL
#define P135_VCOUNT 2


boolean Plugin_135(byte function, struct EventStruct *event, String& string)
{
  boolean success = false;
  int lastStat = -1;

  switch (function)
  {
    case PLUGIN_DEVICE_ADD:
      {
        Device[++deviceCount].Number = PLUGIN_ID_135;
        Device[deviceCount].Type = DEVICE_TYPE_DUAL;
        Device[deviceCount].VType = P135_VTYPE;
        Device[deviceCount].Ports = 0;
        Device[deviceCount].PullUpOption = false;
        Device[deviceCount].InverseLogicOption = true;
        Device[deviceCount].FormulaOption = false;
        Device[deviceCount].ValueCount = P135_VCOUNT;
        Device[deviceCount].SendDataOption = true;
        Device[deviceCount].TimerOption = true;
        Device[deviceCount].TimerOptional = true;
        Device[deviceCount].GlobalSyncOption = true;
        break;
      }

    case PLUGIN_GET_DEVICENAME:
      {
        string = F(PLUGIN_NAME_135);
        break;
      }

    case PLUGIN_GET_DEVICEVALUENAMES:
      {
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_135));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_135));
        ExtraTaskSettings.TaskDeviceValueDecimals[0] = 0;
        ExtraTaskSettings.TaskDeviceValueDecimals[1] = 0;

#if (P135_VTYPE == SENSOR_TYPE_TRIPLE) || (P135_VTYPE == SENSOR_TYPE_QUAD)
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[2], PSTR(PLUGIN_VALUENAME3_135));
        ExtraTaskSettings.TaskDeviceValueDecimals[2] = 4;
#endif
#if (P135_VTYPE == SENSOR_TYPE_QUAD)
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[3], PSTR(PLUGIN_VALUENAME4_135));
        ExtraTaskSettings.TaskDeviceValueDecimals[3] = 4;
#endif

        break;
      }

    case PLUGIN_WEBFORM_LOAD:
      {
        String resultsOptions[2] = { F("1st GPIO = UP, 2nd GPIO = DOWN"), F("1st GPIO = ON/OFF, 2nd GPIO = UP/DOWN") };
    		int resultsOptionValues[2] = { 0, 1 };
    		addFormSelector(F("GPIO Mode"), F("P135_gmode"), 2, resultsOptions, resultsOptionValues, P135_GMODE);
        //string += F("<TR><TD>NOTE:<TD>1st GPIO = UP, 2nd GPIO = DOWN");

        /*
        addFormNumericBox(F("Time from Top to Half (ms)"), F("P135_TTH"), P135_TTH, 1000, 999999);
        addFormNumericBox(F("Time from Top to Bottom (ms)"), F("P135_TTB"), P135_TTB, 1000, 999999);
        addFormNumericBox(F("Time from Top to Closed (ms)"), F("P135_TTC"), P135_TTC, 1000, 999999);
        addFormNumericBox(F("Time from Closed to Botton (ms)"), F("P135_TCB"), P135_TCB, 1000, 999999);
        addFormNumericBox(F("Time from Closed to Half (ms)"), F("P135_TCH"), P135_TCH, 1000, 999999);
        addFormNumericBox(F("Time from Closed to Top (ms)"), F("P135_TCT"), P135_TCT, 1000, 999999);
        */
        const float mil = 1000.0F;
        addFormFloatNumberBox(F("Time from Top to Closed"),F("P135_TTC"), P135_TTC/mil, 5, mil);
        addUnit(F("sec"));
        addFormFloatNumberBox(F("Time from Top to Bottom"),F("P135_TTB"), P135_TTB/mil, 0, mil);
        addUnit(F("sec"));
        addFormFloatNumberBox(F("Time from Top to Half"),F("P135_TTH"), P135_TTH/mil, 0, mil);
        addUnit(F("sec"));
        addFormFloatNumberBox(F("Time from Closed to Top"),F("P135_TCT"), P135_TCT/mil, 0, mil);
        addUnit(F("sec"));
        addFormFloatNumberBox(F("Time from Closed to Botton"),F("P135_TCB"), P135_TCB/mil, 0, mil);
        addUnit(F("sec"));
        addFormFloatNumberBox(F("Time from Closed to Half"),F("P135_TCH"), P135_TCH/mil, 0, mil);
        addUnit(F("sec"));


        addHtml(F("<TR><TD>Position:<TD>0-100; 0: Top, 99: Bottom, 100: Closed"));
        addHtml(F("<TR><TD>Status:<TD>0: Uknown, 1: Stop, 2: Up, 3: Down"));
        /*
        addHtml(F("<TR><TD>RealPos:<TD>0-100-"));
        float maxPos = 0;
        if (P135_TCT <= 0)
          addHtml(F("1xx"));
        else
        {
          maxPos = P135_MAXPOSUD;
          addHtml(String(maxPos));
        }
        addHtml(F("; 0: Top, 100: Bottom, "));
        if (P135_TCT <= 0)
          addHtml(F(">100"));
        else
        {
          addHtml(F(">="));
          addHtml(String(maxPos));
        }
        addHtml(F(": Closed"));
        */
        success = true;
        break;
      }

    case PLUGIN_WEBFORM_SAVE:
      {
        const float mil = 1000.0F;
        P135_GMODE = getFormItemInt(F("P135_gmode"));
        P135_TTC = getFormItemFloat(F("P135_TTC")) * mil;
        P135_TTB = getFormItemFloat(F("P135_TTB")) * mil;
        P135_TTH = getFormItemFloat(F("P135_TTH")) * mil;
        P135_TCT = getFormItemFloat(F("P135_TCT")) * mil;
        P135_TCH = getFormItemFloat(F("P135_TCH")) * mil;
        P135_TCB = getFormItemFloat(F("P135_TCB")) * mil;

        Plugin_135_Init(event);

        success = true;
        break;
      }

    case PLUGIN_INIT:
      {
        Plugin_135_Init(event);

        success = true;
        break;
      }

    case PLUGIN_WRITE:
      {
        String command = parseString(string, 1);
        int dotPos = command.indexOf('.');
        bool istanceCommand = false;
        if(dotPos > -1)
        {
          LoadTaskSettings(event->TaskIndex);
          String name = command.substring(0,dotPos);
          name.replace("[","");
          name.replace("]","");
          if(name.equalsIgnoreCase(getTaskDeviceName(event->TaskIndex)) == true)
          {
            command = command.substring(dotPos + 1);
            istanceCommand = true;
          }
        }
        if (istanceCommand)
        {
          lastStat = P135_STAT;
          if (command == F("stop"))
          {
            success = true;
            Plugin_135_Action(event,P135_STOP);
            if (P135_STAT >= P135_UK_STOP)
              P135_STAT = P135_UK_STOP;
            else
              P135_STAT = P135_STOP;
            P135_PREVPOS = -123;
          }
          else if ((command == F("open"))  || ((command == F("up")) && (event->Par1 == 0)))
          {
            success = true;
            int newstat;
            if (P135_STAT >= P135_UK_STOP)
            {
              P135_RPOS = P135_MAXPOSDU;
              P135_MSPOS = P135_TCT;
              newstat = P135_UK_UP;
            }
            else
            {
              P135_MSPOS = Plugin_135_getPos_ms(P135_RPOS,P135_TCT - P135_TCH, P135_TCT - P135_TCB);
              newstat = P135_UP;
            }
            P135_FPOS = 0;
            Plugin_135_Action(event,P135_UP);
            P135_STAT = newstat;
          }
          else if ((command == F("close"))  || ((command == F("down")) && (event->Par1 == 0)))
          {
            success = true;
            int newstat;
            if (P135_STAT >= P135_UK_STOP)
            {
              P135_RPOS = 0;
              P135_MSPOS = 0;
              newstat = P135_UK_DOWN;
            }
            else
            {
              P135_MSPOS = Plugin_135_getPos_ms(P135_RPOS, P135_TTH, P135_TTB);
              newstat = P135_DOWN;
            }
            P135_FPOS = 101;
            Plugin_135_Action(event,P135_DOWN);
            P135_STAT = newstat;
          }
          else if ((command == F("set")) || (command == F("up")) || (command == F("down")))
          {
            success = true;
            P135_FPOS = event->Par1;
            if (event->Par1 < 2)
              P135_FPOS = 0;
            else if (event->Par1 == 99)
              P135_FPOS = 100;
            else if (event->Par1 >= 100)
              P135_FPOS = 101;
            if (P135_STAT >= P135_UK_STOP)
            {
              if (P135_FPOS > 50)
              {
                P135_RPOS = 0;
                P135_MSPOS = 0;
                Plugin_135_Action(event,P135_DOWN);
                P135_STAT = P135_UK_DOWN;
              }
              else
              {
                P135_RPOS = P135_MAXPOSDU;
                P135_MSPOS = P135_TCT;
                Plugin_135_Action(event,P135_UP);
                P135_STAT = P135_UK_UP;
              }
            }
            else if (((P135_RPOS - P135_FPOS) >= 5) && ((command == F("set")) || (command == F("up"))))
            {
              P135_MSPOS = Plugin_135_getPos_ms(P135_RPOS,P135_TCT - P135_TCH, P135_TCT - P135_TCB);
              Plugin_135_Action(event,P135_UP);
              P135_STAT = P135_UP;
            }
            else if (((P135_FPOS - P135_RPOS) >= 5) && ((command == F("set")) || (command == F("down"))))
            {
              P135_MSPOS = Plugin_135_getPos_ms(P135_RPOS, P135_TTH, P135_TTB);
              Plugin_135_Action(event,P135_DOWN);
              P135_STAT = P135_DOWN;
            }
          }
        }
        break;
      }

    case PLUGIN_READ:
      {
        Plugin_135_READ(event);
        success = true;
        break;
      }

    //case PLUGIN_TEN_PER_SECOND:
    case PLUGIN_FIFTY_PER_SECOND:
      {
        if (((P135_STAT % 10) > P135_STOP) || (P135_PREVPOS == -123))
        {
          if (P135_PREVPOS != -123)
          {
            lastStat = P135_STAT;
            Plugin_135_Update(event,false);
          }
          int ActPos = P135_RPOS / 10;
          if ((P135_PREVPOS != ActPos) || ((P135_STAT % 10) == P135_STOP))
          {
            P135_PREVPOS = ((P135_STAT % 10) == P135_STOP)? -1: ActPos;
            Plugin_135_sendData(event,((P135_STAT % 10) > P135_STOP));
          }
        }
        success = true;
        break;
      }

  }

  if (success && lastStat >= 0)
  {
    boolean isRunning = ((P135_STAT % 10) > P135_STOP);
    if (isRunning != ((lastStat % 10) > P135_STOP))
    {
      P135_XSTAT = (P135_STAT == P135_UK_STOP ? P135_UKNOWN : P135_STAT % 10);
      String eventString = F("Roller#");
      eventString += isRunning ? F("Start") : F("Stop");
      eventString += '=';
      eventString += (event->TaskIndex + 1);
      rulesProcessing(eventString);
    }
  }

  return success;
}

void Plugin_135_Action(struct EventStruct *event, int8_t pAct)
{
  pAct = pAct % 10;
  if ((P135_STAT % 10) == pAct)
    return;

  int8_t st1 = (P135_LOWACTIVE? HIGH: LOW);
  int8_t st2 = st1;

  if ((P135_STAT % 10) > P135_STOP)
  {
    Plugin_135_Update(event,true);
  }

  if (((pAct % 10) == P135_STOP) || (P135_GMODE == 0)) // Stop (Off)
  {
    digitalWrite(P135_PIN1, st1);
    //setPinState(PLUGIN_ID_135, P135_PIN1, PIN_MODE_OUTPUT, (P135_LOWACTIVE? HIGH: LOW));
    digitalWrite(P135_PIN2, st2);
    //setPinState(PLUGIN_ID_135, P135_PIN2, PIN_MODE_OUTPUT, (P135_LOWACTIVE? HIGH: LOW));
  }
  //P135_STAT = pAct;
  //pAct = pAct % 10;
  P135_MILLIS = millis();
  if (pAct == P135_UP)                              // Up
  {
    if (P135_GMODE == 0)
    {
      st1 = (P135_LOWACTIVE? LOW: HIGH);
      digitalWrite(P135_PIN1, st1);
      //setPinState(PLUGIN_ID_135, P135_PIN1, PIN_MODE_OUTPUT, (P135_LOWACTIVE? LOW: HIGH));
    }
    else
    {
      st2 = (P135_LOWACTIVE? LOW: HIGH);
      digitalWrite(P135_PIN2, st2);
      //setPinState(PLUGIN_ID_135, P135_PIN2, PIN_MODE_OUTPUT, (P135_LOWACTIVE? LOW: HIGH));
    }
  }
  else if (pAct == P135_DOWN)                         // Down
  {
    if (P135_GMODE == 0)
    {
      st2 = (P135_LOWACTIVE? LOW: HIGH);
      digitalWrite(P135_PIN2, st2);
      //setPinState(PLUGIN_ID_135, P135_PIN2, PIN_MODE_OUTPUT, (P135_LOWACTIVE? LOW: HIGH));
    }
    else
    {
      st2 = (P135_LOWACTIVE? HIGH: LOW);
      digitalWrite(P135_PIN2, st2);
      //setPinState(PLUGIN_ID_135, P135_PIN2, PIN_MODE_OUTPUT, (P135_LOWACTIVE? HIGH: LOW));
    }
  }
  if ((pAct > P135_STOP) && (P135_GMODE == 1)) // On
  {
    st1 = (P135_LOWACTIVE? LOW: HIGH);
    digitalWrite(P135_PIN1, st1);
    //setPinState(PLUGIN_ID_135, P135_PIN1, PIN_MODE_OUTPUT, (P135_LOWACTIVE? LOW: HIGH));
  }

  portStatusStruct newStatus;
  const uint32_t key1 = createKey(PLUGIN_ID_135,P135_PIN1);
  const uint32_t key2 = createKey(PLUGIN_ID_135,P135_PIN2);
  // WARNING: operator [] creates an entry in the map if key does not exist
  newStatus = globalMapPortStatus[key1];
  newStatus.command=1;
  newStatus.mode = PIN_MODE_OUTPUT;
  newStatus.state = st1;
  savePortStatus(key1,newStatus);
  newStatus = globalMapPortStatus[key2];
  newStatus.command=1;
  newStatus.mode = PIN_MODE_OUTPUT;
  newStatus.state = st2;
  savePortStatus(key2,newStatus);
}

void Plugin_135_Update(struct EventStruct *event, boolean Only)
{
  if ((P135_STAT % 10) == P135_STOP)
    return;

  //unsigned long millisAct = millis();
  long posAdv = millis() - P135_MILLIS;
  if ((P135_STAT % 10) == P135_UP)
  {
    P135_RPOS = Plugin_135_getRPos(P135_MSPOS - posAdv, P135_TCT - P135_TCH, P135_TCT - P135_TCB);
    if (Only)
    {
      P135_MSPOS -= posAdv;
      return;
    }
    //if (((P135_FPOS == 0) && (posAdv >= (P135_TTC + P135_TTC/10))) || ((P135_FPOS != 0) && (P135_RPOS <= P135_FPOS)))
    if ((((P135_STAT >= P135_UK_STOP) || (P135_FPOS <= 0)) && (posAdv >= (P135_TCT + P135_TCT/10))) || ((P135_STAT < P135_UK_STOP) && (P135_RPOS <= P135_FPOS)))
    {
      if (P135_STAT >= P135_UK_STOP)
        P135_RPOS = 0;
      Plugin_135_Action(event,P135_STOP);
      P135_STAT = P135_STOP;
    }
  }
  else if ((P135_STAT % 10) == P135_DOWN)
  {
    P135_RPOS = Plugin_135_getRPos(P135_MSPOS + posAdv, P135_TTH, P135_TTB);
    if (Only)
    {
      P135_MSPOS += posAdv;
      return;
    }
    //if (((P135_FPOS == P135_MAXPOS/10) && (posAdv >= (P135_TTC + P135_TTC/10))) || ((P135_FPOS != P135_MAXPOS/10) && (P135_RPOS >= P135_FPOS)))
    if ((((P135_STAT >= P135_UK_STOP) || (P135_FPOS > 100)) && (posAdv >= (P135_TTC + P135_TTC/10))) || ((P135_STAT < P135_UK_STOP) && (P135_RPOS >= P135_FPOS)))
    {
      if (P135_STAT >= P135_UK_STOP)
        P135_RPOS = P135_MAXPOSDU;
      Plugin_135_Action(event,P135_STOP);
      P135_STAT = P135_STOP;
    }
  }
  if (P135_STAT == P135_STOP)
  {
    if ((P135_RPOS - P135_FPOS) >= 5)
    {
      P135_MSPOS = Plugin_135_getPos_ms(P135_RPOS,P135_TCT - P135_TCH, P135_TCT - P135_TCB);
      Plugin_135_Action(event,P135_UP);
      P135_STAT = P135_UP;
    }
    else if ((P135_FPOS - P135_RPOS) >= 5)
    {
      P135_MSPOS = Plugin_135_getPos_ms(P135_RPOS, P135_TTH, P135_TTB);
      Plugin_135_Action(event,P135_DOWN);
      P135_STAT = P135_DOWN;
    }
  }
}

void Plugin_135_sendData(struct EventStruct *event, boolean NoRules)
{
  static unsigned long nextSend = 0;

  int pac = Plugin_135_READ(event);
  event->sensorType = P135_VTYPE;

  String log = F("INFO : Roller: Task ");
  log += event->TaskIndex;
  log += F(", msPos ");
  log += P135_MSPOS;
  log += F(", Stat ");
  log += P135_STAT;
  log += F(", pac ");
  log += pac;
  log += F(", RPos ");
  log += P135_RPOS;
  addLog(LOG_LEVEL_INFO, log);

  if ((pac != P135_PAC) || (millis() > nextSend) || (!NoRules))
  {
    P135_PAC = pac;
    nextSend = millis() + 10000;
    boolean prevUR = Settings.UseRules;
    if (NoRules)
      Settings.UseRules = false;
    sendData(event);
    if (NoRules)
      Settings.UseRules = prevUR;
  }
}

int Plugin_135_READ(struct EventStruct *event)
{
  float pos;
  if (P135_STAT == P135_UK_STOP)
  {
    P135_POS = 0;
    pos = 999;
  }
  else
  {
    pos = int(P135_RPOS + 0.4);
    if (pos < 0)
      P135_POS = 0;
    else if (pos < 99)
      P135_POS = pos;
    else if (pos <= 100)
      P135_POS = 99;
    else
      P135_POS = 100;
  }
  P135_XSTAT = (P135_STAT == P135_UK_STOP ? P135_UKNOWN : P135_STAT % 10);
  return (P135_STAT * 1000 + pos);
}

/*
float Plugin_135_getiPos(struct EventStruct *event, long lPos)
{
  float x = (float) constrain(lPos, 0, P135_TTC);
  float x1 = (float) P135_TTH;
  float x2 = (float) P135_TTB;
  return (50.0*x*(x-x2)/(x1*(x1-x2))+100.0*x*(x-x1)/(x2*(x2-x1)));
}

int Plugin_135_getiPosi(long x)
{
  long x1 = P135_TTH / 10L;
  long x2 = P135_TTB / 10L;
  return (50L*x*(x/100L-x2)/(x1*(x1-x2))+100L*x*(x/100L-x1)/(x2*(x2-x1)));
}
*/

float Plugin_135_getRPos(float x, float x1, float x2)
{
  /*
  Down:
  float x = (float) constrain(lPos, 0, P135_TTC);
  float x1 = (float) P135_TTH;
  float x2 = (float) P135_TTB;
  Up:
  float x = (float) constrain(lPos, 0, P135_TCT);
  float x1 = (float) P135_TCT - (float) P135_TCH;
  float x2 = (float) P135_TCT - (float) P135_TCB;
  */
  return (50.0*x*(x-x2)/(x1*(x1-x2))+100.0*x*(x-x1)/(x2*(x2-x1)));
}

float Plugin_135_getPos_ms(float y, float x1, float x2)
{
  /*
  y = constrain(y, 0, P135_MAXPOS/10);
  Down:
  float x1 = (float) P135_TTH;
  float x2 = (float) P135_TTB;
  Up:
  float x1 = (float) P135_TCT - (float) P135_TCH;
  float x2 = (float) P135_TCT - (float) P135_TCB;
  */
  float K = x1*(x1 - x2);
  float L = x2*(x2 - x1);
  float a = 50*L + 100*K;
  float b = 50*L*x2 + 100*K*x1;
  float c = K*L*y;
  if (a == 0)
    return (-c/b);
  else
    return ((b - sqrt(b*b + 4*a*c))/(2*a));
}




void Plugin_135_Init(struct EventStruct *event)
{
  long P135_dft;

  P135_dft = P135_TTC;
  if (P135_dft == 0)
    P135_dft = P135_TCT;
  if (P135_dft == 0)
    P135_dft = P135_TTB;
  if (P135_dft == 0)
    P135_dft = P135_TTH * 2;
  if (P135_dft == 0)
    P135_dft = 20000;

  if (P135_TTC == 0)
    P135_TTC = P135_dft;
  if (P135_TCT == 0)
    P135_TCT = P135_TTC;
  if (P135_TTB == 0 || P135_TTB > P135_TTC)
    P135_TTB = P135_TTC;
  if (P135_TTH == 0 || P135_TTH >= P135_TTB)
    P135_TTH = P135_TTB / 2;
  if (P135_TCB >= P135_TCT)
    P135_TCB = map(P135_TTB, 0, P135_TTC, P135_TCT, 0);
  if (P135_TCH == 0 || P135_TCH >= P135_TCT || P135_TCH <= P135_TCB)
    P135_TCH = map(P135_TTH, 0, P135_TTC, P135_TCT, 0);

/*
  String log = F("INIT0: Roller: Task ");
  log += event->TaskIndex;
  log += F(", Pin1 ");
  log += P135_PIN1;
  log += F(", Pin2 ");
  log += P135_PIN2;
  log += F(", Pos ");
  log += P135_POS;
  log += F(", XStat ");
  log += P135_XSTAT;
  log += F(", Stat ");
  log += P135_STAT;
  log += F(", RPos ");
  log += P135_RPOS;
  log += F(", FPos ");
  log += P135_FPOS;
  log += F(", maxPosUD ");
  log += P135_MAXPOSUD;
  log += F(", maxPosDU ");
  log += P135_MAXPOSDU;
  addLog(LOG_LEVEL_INFO, log);
*/

  //P135_MAXPOSUD = Plugin_135_getRPos(P135_TTC,P135_TTH,P135_TTB)*100.0F;
  //P135_MAXPOSDU = Plugin_135_getRPos(P135_TCT,P135_TCT-P135_TCH,P135_TCT-P135_TCB)*100.0F;

  Plugin_135_Action(event,P135_STOP);
  if (P135_XSTAT == P135_UKNOWN)
    P135_STAT = P135_UK_STOP;
  else
  {
    P135_STAT = P135_XSTAT;
    if (P135_STAT > P135_STOP)
    {
      P135_STAT = P135_UK_STOP;
      if (P135_FPOS > 50)
      {
        P135_RPOS = 0;
        P135_MSPOS = 0;
        Plugin_135_Action(event,P135_DOWN);
        P135_STAT = P135_UK_DOWN;
      }
      else
      {
        P135_RPOS = P135_MAXPOSDU;
        P135_MSPOS = P135_TCT;
        Plugin_135_Action(event,P135_UP);
        P135_STAT = P135_UK_UP;
      }
    }
  }

  pinMode(P135_PIN1, OUTPUT);
  pinMode(P135_PIN2, OUTPUT);

  Plugin_135_READ(event);

  String log = F("INIT : Roller: Task ");
  log += event->TaskIndex;
  log += F(", Pin1 ");
  log += P135_PIN1;
  log += F(", Pin2 ");
  log += P135_PIN2;
  log += F(", Pos ");
  log += P135_POS;
  log += F(", XStat ");
  log += P135_XSTAT;
  log += F(", Stat ");
  log += P135_STAT;
  log += F(", RPos ");
  log += P135_RPOS;
  log += F(", FPos ");
  log += P135_FPOS;
  log += F(", maxPosUD ");
  log += P135_MAXPOSUD;
  log += F(", maxPosDU ");
  log += P135_MAXPOSDU;
  addLog(LOG_LEVEL_INFO, log);

#if (P135_VTYPE == SENSOR_TYPE_TRIPLE) || (P135_VTYPE == SENSOR_TYPE_QUAD)
  ExtraTaskSettings.TaskDeviceValueDecimals[2] = 4;
#endif
#if (P135_VTYPE == SENSOR_TYPE_QUAD)
  ExtraTaskSettings.TaskDeviceValueDecimals[3] = 4;
#endif

}

//#endif // USES_P135
