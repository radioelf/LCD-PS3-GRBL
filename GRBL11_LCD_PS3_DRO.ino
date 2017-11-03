/*'******************************************************************
'*  Name    : GRBL universal DRO (grudr11)                          *
'*  Author  : jpbbricole                                            *
'*  Date    : 28.11.2016                                            *
'*  Version : projectVersion                                        *
'*  Notes   : Requests (?) or receives automatically (UGS)          *
'*            status line from GRBL Since 1.1 (S11)
'*                                                                  *
'* This version needs Arduino with multiple hardware serial ports   *
'* Arduino Leonardo or greater.                                     *
'*            After treatment status data are                       *
'*            sent to a LCD i2C Display                             *
'*            and optionally to a DRO (Excel sheet in this example) *
'*                                                                  *
'*            you can send commands to the program,                 *
'*        see the Monitor subroutines.                              *
'* https://www.shapeoko.com/wiki/index.php/LCD_on_GRBL              *
'* ---------------------- GRBL since 1.1 (S11) -------------------- *
'* GRBL Version Grbl 1.1 and 115200 and UGS 2.0                     *
'* Setting GRBL:                                                    *
*' $10=1 it displays Feed and Speed                                 *
*' $10=2 it displays Buffer, Feed and Speed                         *
'* --------------------- GRBL documentation ----------------------- *
'* https://github.com/gnea/grbl/tree/master/grbl                    *
'********************************************************************
'* MOD. Radioelf for control PS3 Dualshock 3 --9/10/17--
*/
//#define DRO
//#define PS3_debug

#include <Wire.h>                                          // Comes with Arduino IDE
#include <LiquidCrystal_I2C.h>
#ifndef DRO
  #include <PS3USB.h>                                      //  https://github.com/felis/USB_Host_Shield_2.0  https://felis.github.io/USB_Host_Shield_2.0/class_p_s3_u_s_b.html
  #include <SPI.h>
  USB Usb;
  PS3USB PS3(&Usb); 
#endif
LiquidCrystal_I2C lcd(0x27,20, 4);

#define  grblFormatDecimalNumbers 2
enum grblS11StatusValuesIndex {grblS11ValStatus, grblS11MachX, grblS11MachY, grblS11MachZ, grblS11BufBlocks, grblS11BufBytes, grblS11Feed, grblS11Spindle, 
	                           grblS11WorkX, grblS11WorkY, grblS11WorkZ, grblS11ValLasted};
const int grblStatusValuesDim = grblS11ValLasted;
String grblStatusValues[grblS11ValLasted];

enum grblS11AxisValFloatIndex {grblS11axisX, grblS11axisY, grblS11axisZ, grblS11axisLasted};
int grblS11AxisValIntMpos[grblS11axisLasted];
int grblS11AxisValIntWpos[grblS11axisLasted];

enum grblS11WcoValuesIndex {grblS11WcoX, grblS11WcoY, grblS11WcoZ, grblS11WcoLasted};
int grblS11WcoValInt[grblS11WcoLasted];                    // WCO floating values

int grblS11PosMode;                                        // Working Pos WPos  or  Machine Pos MPos
String grblS11PosModeStr;
enum grblS11PosModeIndex {grblS11posMach, grblS11posWork, grblS11posLasted};
	
String grblStatusPrevious = "";                            // thes variables to avoid sending values that do not change
boolean grblStatusTimeToDisplay;                           // If time to display or send GRBL status to display or DRO
                                                           // To avoid clutter in the rapid arrival of status (UGS)

String grblStatusExtraPrevious = "";                       // Contain WCO: 0v: and other extra data                           
String grblStatusExtra;
String grblStatusExtraWco;

unsigned long grblPollingTimer;
int grblPollingPeriod = 200;                               // For every 250 mS

unsigned long grblActivityWatchdog;                        // Timer for watching GRBL data line activity
int grblActivityWatchdogPeriod;

#define grblTxSwitch 7                                     // Electronic GRBL Tx switch command
boolean grblStringAvailable = false;                       // If a complete string received from GRBL
String  grblStringRx = "";                                 // Received string
                                 
int grblVersion = 0;
enum grblVersionsIndex {grblVersionUnknown, grblVersionSince11, grblVersionLasted};
// Internal Parameters
boolean interParModeAuto = true;                           // Function mode program true(default): the program waits GRBL's data
                                                           // if you send a Gcode file, it is imperative to be interParModeAuto = true
                                                           // otherwise the polling might create transmission errors.
#ifdef DRO                                                          
  boolean interParDroConnected = true;                     // If a DRO connected (internal parameter false default)----------
  String interParDroIdentity = "DROX";
#endif
//     Monitor Parameters
bool monCommandNew = false;                                // If a new command received from IDE Arduino monitoe
String monCommand = "";                                    // received command
//     Tools Parameters
#define  SplittingArrayDim grblS11ValLasted
String   SplittingArray[SplittingArrayDim];

boolean grudrDebugOn = false;


// control Dualshock 3 PS3
bool Control_PS3 =false;
#ifndef DRO
  bool run_joystick = false;
  unsigned long joystick_run;
  unsigned int velocidad =250;                           // velocidad defecto bajar/subir eje X  
#endif
  
void setup()
{
byte espera =0;
String  InfoDebug = "";
	Serial.begin(115200);                                   // IDE monitor or DRO serial-> debug + DRO
	Serial1.begin(115200);                                  // GRBL Communication-> TX a GRBL
	lcd.begin();
	lcd.noBacklight();

	pinMode(grblTxSwitch, OUTPUT);
	digitalWrite(grblTxSwitch, LOW);
  #ifndef DRO
    pinMode(LED_BUILTIN, OUTPUT);
  #endif
  grblStatusTimeToDisplay = false;
  delay(500);
  lcd.backlight();
  //***********************************
#ifndef DRO
  if (Usb.Init() !=-1) {
    InfoDebug ="Controlador PS3 OK";
  }else{
    InfoDebug ="NO controlador PS3";
  }
  lcd.setCursor(0, 0);
  lcd.print(InfoDebug);
  #ifdef PS3_debug
    Serial.println(InfoDebug);
  #endif
  delay(2500);
  while (espera <11){
    Usb.Task();
    delay(100);
    if (PS3.PS3Connected){ 
      Control_PS3 =true;
      InfoDebug = "Dualshock 3 ON";
      espera =19;
    }
    espera++;
  }
  if (espera !=20){
    InfoDebug = "Dualshock 3 OFF";
  }
  #ifdef PS3_debug
    Serial.println(InfoDebug);
  #endif
  lcd.setCursor(0, 1);
  lcd.print(InfoDebug);
  delay(2500);
  lcd.clear();
  delay(100);
#endif
  //***********************************
  #ifdef PS3_debug
	  Serial.println("GRUDR11 0.5-Mod.");
	#endif
	grblStatusDisplay();
	lcdPrint("GRUDR11 0.5-Mod.", 0, 0);
  delay(2500);
  grblActivityWatchdog = millis(); 
  grblPollingTimer = millis();
  grblActivityWatchdogSet();
}
//********************************************************************************
void loop()
{
	if (millis() - grblPollingTimer > grblPollingPeriod)                // GRBL status timout
	{
		if (!interParModeAuto){grblStatusRequest();}
		grblStatusTimeToDisplay = true;
		
		grblPollingTimerRestart();
    #ifndef DRO
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    #endif
	} 
  //***********************************
  #ifndef DRO
  if (Control_PS3){
    Usb.Task();
    Dualshock3_RX();
    if (run_joystick == true){                                          // si tenemos movimiento con el joystick
      if(millis() > joystick_run+100) run_joystick = false;             // si a transcurrido el periodo de 200ms
    } 
  }
  #endif
  //***********************************  
	serial1Event();                                                       // GRBL communications
	if (grblStringAvailable)
	{
		grblStringAvailable = false;
		grblRxEvaluation(grblStringRx);
    #ifdef PS3_debug
      Serial.println(grblStringRx);
    #endif
		grblStringRx = "";
		grblActivityTimerRestart();
	}

	serialEvent();
	if (monCommandNew)                                                      // Arduino IDE monitor
	{
		monCommTreatment(monCommand);
		monCommand = "";
		monCommandNew  = false;
	}
}
//********************************************************************************
//------------------------------------ GRBL subroutines
void grblStatusRequest()
{
	if (!grblActivityIfActive())
	{
    #ifdef PS3_debug
		  Serial.println("Status ?");
    #endif
		grblCmdSend("?");
	}
}

void grblPollingTimerRestart()
{
	grblPollingTimer = millis();
}

void grblActivityWatchdogSet()
{
	grblActivityWatchdogPeriod = grblPollingPeriod-50;
}

void grblCmdSend(String txCmd)
{
	if (!interParModeAuto)
	{
		digitalWrite(grblTxSwitch, HIGH);
		delay(50);
		Serial1.print(txCmd + '\r');
		delay(50);
		digitalWrite(grblTxSwitch, LOW);
	}
	else
	{
    #ifdef PS3_debug
		  Serial.println("Mode auto! (" + txCmd+ ")")	;
    #endif
	}
}

void grblRxEvaluation(String grblRx)
{
	if (grblRx.startsWith("<"))                                             // Status GRBL
	{
		int startStatPos = grblRx.indexOf("<");                               // Cleans the status line.
		int endStatPos = grblRx.indexOf(">");
		grblRx = grblRx.substring(startStatPos+1, endStatPos-0);

		if (grblRx.indexOf("|") >= 0)                                         // GRBL version Since version 1.1 (S11)
		{
			grblVersion = grblVersionSince11;
			grblStatusRxS11(grblRx);
		}
		else                                                                   
		{
			grblVersion = grblVersionUnknown;
		}
	}
	else if (grblRx == "ok")
	{
    
	}
	else
	{
    #ifdef PS3_debug
		  Serial.println("!!! GRBL " + grblRx);
    #endif
	}
  if (Control_PS3 && !grblVersion){
    lcdPrintLength ("Grbl:" + grblStringRx, 0, 0,20);
    if (grblStringRx.length() >15) lcdPrintLength (grblStringRx.substring(15,36), 0, 1,20);
    else lcdClsRow(1);
  }
}
/*------------------------------------------------------------------------------
	The status line can take these various states:
	$10=1
	<Idle|MPos:0.000,0.000,0.000|FS:0,0|WCO:0.000,0.000,0.000>
	<Idle|MPos:0.000,0.000,0.000|FS:0,0|Ov:100,100,100>
	<Idle|MPos:0.000,0.000,0.000|FS:0,0>
	$10=2
	<Idle|WPos:0.000,0.000,0.000|Bf:15,128|FS:0,0|WCO:0.000,0.000,0.000>
	<Idle|WPos:0.000,0.000,0.000|Bf:15,128|FS:0,0|Ov:100,100,100>
	<Idle|WPos:0.000,0.000,0.000|Bf:15,128|FS:0,0>
--------------------------------------------------------------------------------
*/
void grblStatusRxS11(String grblStat)
{
	static boolean statNew;
	static byte statNoNew;
	String grblStatOrigin = grblStat;                                       // For Status values array
	grblStat.toUpperCase();

	/*-----------------------------------------------------------
		Separate data after FS:, Ov: WCO: Pn: etc.(see GRBL doc)
		as StatusExtra for treatment
	'*-----------------------------------------------------------
	*/
	int fsPos = grblStat.indexOf("|F");                                     // Search the end of status standard status data
	fsPos = grblStat.indexOf("|", fsPos+2);
	
	grblStatusExtra = "";
	if (fsPos >= 0)
	{
		grblStatusExtra = grblStat.substring(fsPos+1);                        // Extract the extra status data
	}
	grblStat = grblStat.substring(0, fsPos);                                // Delete extra data from status line

	grblS11PosMode = grblS11posMach;
	if(grblStat.indexOf("WPOS") >=0){grblS11PosMode = grblS11posWork;}      // Find if a Mpos or a Wpos status line
			
	grblStat.replace("BF:","");                                             // clean up unnecessary data
	grblStat.replace("|",",");
	grblStat.replace("MPOS:","");                                          
	grblStat.replace("WPOS:","");
	grblStat.replace("FS:","");

  #ifdef PS3_debug
	  if (grudrDebugOn){Serial.println(grblStatOrigin); Serial.println(grblStat);}
  #endif
	if (grblStat != grblStatusPrevious)
	{
		grblStatusPrevious = grblStat;
		statNew = true;
		statNoNew = 0;
	}
	else
	{
		statNew = false;
		statNoNew ++;
		if (statNoNew > 10)                                                     // For if there is no new status, refresh display
		{
			statNoNew = 0;
			statNew = true;
		}
	}
	toSplitCommand(grblStat, ",");
	
	for(int i = 0; i < grblStatusValuesDim; i++)
	{
		grblStatusValues[i] = SplittingArray[i];
	}
	// Fill Mpos and Wpos arrays with the same received values
	grblS11AxisValIntMpos[grblS11axisX] = grblValuesStrFloatToInt(grblStatusValues[grblS11MachX]);// It is easier to work integers than floating values
	grblS11AxisValIntMpos[grblS11axisY] = grblValuesStrFloatToInt(grblStatusValues[grblS11MachY]);// So I work with 1/100 mm
	grblS11AxisValIntMpos[grblS11axisZ] = grblValuesStrFloatToInt(grblStatusValues[grblS11MachZ]);// I get this -0.200,3.010,10.000 from GRBL 
	                                                                                              // I put this -20,301,1000 in array
	grblS11AxisValIntWpos[grblS11axisX] = grblS11AxisValIntMpos[grblS11axisX];
	grblS11AxisValIntWpos[grblS11axisY] = grblS11AxisValIntMpos[grblS11axisY];
	grblS11AxisValIntWpos[grblS11axisZ] = grblS11AxisValIntMpos[grblS11axisZ];

	if (grblStatusExtra != "")
	{
		if (grblStatusExtra.startsWith("WCO:"))
		{
			grblStatusExtraWco = grblStatusExtra;
		}
    #ifdef PS3_debug
		  if (grudrDebugOn){Serial.println("Extra: " + grblStatusExtra);}
    #endif
	}
	grblStatusWcoTreatment(grblStatusExtraWco);                                                    // Add WCO values to Mpos or Wpos values

	if (grblStatusTimeToDisplay && statNew)
	{
    #ifdef PS3_debug
		  Serial.println("Stat: " + grblStat);
    #endif
		grblStatusDisplay();
    #ifdef DRO
		  if (interParDroConnected) 
		  {
			  droStatusGrblU11send();
			  droStatusOtherSend();
		  }
    #endif
		grblStatusTimeToDisplay = false;
	}
}

void grblStatusWcoTreatment(String wcoValues)                                                     // WCO:0.000,0.000,0.000
{
	wcoValues.replace("WCO:", "");

	toSplitCommand(wcoValues, ",");
	
	for(int i = 0; i < grblS11WcoLasted; i++)
	{
		grblS11WcoValInt[i] = grblValuesStrFloatToInt(SplittingArray[i]); 
	}

	switch (grblS11PosMode)
	{
		case grblS11posMach:                                                                          // If Machine positions received create working position
			grblS11AxisValIntWpos[grblS11axisX] -= grblS11WcoValInt[grblS11WcoX]; 
			grblS11AxisValIntWpos[grblS11axisY] -= grblS11WcoValInt[grblS11WcoY];
			grblS11AxisValIntWpos[grblS11axisZ] -= grblS11WcoValInt[grblS11WcoZ];
            // Moves Speed and Spindle to their array position.
            grblStatusValues[grblS11Feed] = grblStatusValues[grblS11BufBlocks];
            grblStatusValues[grblS11Spindle] = grblStatusValues[grblS11BufBytes];
			grblStatusValues[grblS11BufBlocks] = "-";
			grblStatusValues[grblS11BufBytes] = "-";
			break;
		case grblS11posWork:                                                                          // If Machine positions received creat working position
			grblS11AxisValIntMpos[grblS11axisX] += grblS11WcoValInt[grblS11WcoX];
			grblS11AxisValIntMpos[grblS11axisY] += grblS11WcoValInt[grblS11WcoY];
			grblS11AxisValIntMpos[grblS11axisZ] += grblS11WcoValInt[grblS11WcoZ];
			break;
	}
}

int grblValuesStrFloatToInt(String strFloatValue)
{
	return (strFloatValue.toFloat())*pow(10, grblFormatDecimalNumbers);
}

String grblValuesIntToStrFloat(int IntValue)
{
	float retVal = (float)IntValue / pow(10, grblFormatDecimalNumbers);
	return (String)retVal;
}

void grblActivityTimerRestart()
{
	grblActivityWatchdog = millis();
}

boolean grblActivityIfActive()
{
	boolean actStat = true;
	
	if (millis() - grblActivityWatchdog > grblActivityWatchdogPeriod) {actStat = false;}
	
	return actStat;
}

void grblStatusDisplay()
{
	static int actualVersion;
	
	if (grblVersion != actualVersion)
	{
		lcdCls();
		actualVersion = grblVersion;
	}
	
	switch (grblVersion)
	{
		case grblVersionSince11:
			grblStatusDisplayS11();
			break;
		default:
			lcdCls();
     if (Control_PS3){
      lcdPrint("Control manual.", 0, 2);
     }else{
			lcdPrint("Esperando datos", 0, 2);
     }
			break;
	}	
}

void grblStatusDisplayS11()
{
	static int actualPosMode;
	
	if (grblS11PosMode != actualPosMode)                                              // tenemos cambio actualizamos display
	{
		lcdCls();
		actualPosMode = grblS11PosMode;
	}
	
	switch (grblS11PosMode)
	{
		case grblS11posMach:
			grblS11PosModeStr = "m";
			lcdPrintLength("F" + grblStatusValues[grblS11Feed], 6, 0, 5);
			lcdPrintLength("S" + grblStatusValues[grblS11Spindle], 12, 0, 7);
			break;
		case grblS11posWork:
			grblS11PosModeStr = "w";
			lcdPrintLength("B" + grblStatusValues[grblS11BufBlocks], 6, 0, 3);
			lcdPrintLength("F" + grblStatusValues[grblS11Feed], 10, 0, 4);
			lcdPrintLength("S" + grblStatusValues[grblS11Spindle], 15, 0, 5);
			break;
		default:
			grblS11PosModeStr = "? ";
			break;
	}
	lcdPrintLength(grblStatusValues[grblS11ValStatus], 0,0, 6);                      // columna 0, linea 0, longitud texto 6
	lcdPrintLength(grblS11PosModeStr, 19,3, 1);

	lcdPrintLength("Xwm " + grblValuesIntToStrFloat(grblS11AxisValIntWpos[grblS11axisX]), 0,1, 11); lcdPrintLength(grblValuesIntToStrFloat(grblS11AxisValIntMpos[grblS11axisX]), 12, 1, 8);
	lcdPrintLength("Ywm " + grblValuesIntToStrFloat(grblS11AxisValIntWpos[grblS11axisY]), 0,2, 11); lcdPrintLength(grblValuesIntToStrFloat(grblS11AxisValIntMpos[grblS11axisY]), 12, 2, 8);
	lcdPrintLength("Zwm " + grblValuesIntToStrFloat(grblS11AxisValIntWpos[grblS11axisZ]), 0,3, 11); lcdPrintLength(grblValuesIntToStrFloat(grblS11AxisValIntMpos[grblS11axisZ]), 12, 3, 7);
}
#ifdef DRO
////////////////////////////////////DRO//////////////////////////////////////////
void droStatusGrblU11send()
{
	//---------------------------- GRBL U11 Status
	  droSendValues("GSTAT",1,grblStatusValues[grblS11ValStatus]);
	  droSendValues("GBUFF",1,grblStatusValues[grblS11BufBlocks]);
	  droSendValues("GFEED",1,grblStatusValues[grblS11Feed]);
	  droSendValues("GSPIN",1,grblStatusValues[grblS11Spindle]);
	//---------------------------- Machine positions
	  droSendValues("GMX",1,grblValuesIntToStrFloat(grblS11AxisValIntMpos[grblS11axisX]));
	  droSendValues("GMY",1,grblValuesIntToStrFloat(grblS11AxisValIntMpos[grblS11axisY]));
	  droSendValues("GMZ",1,grblValuesIntToStrFloat(grblS11AxisValIntMpos[grblS11axisZ]));
	//---------------------------- Working positions
	  droSendValues("GWX",1,grblValuesIntToStrFloat(grblS11AxisValIntWpos[grblS11axisX]));
	  droSendValues("GWY",1,grblValuesIntToStrFloat(grblS11AxisValIntWpos[grblS11axisY]));
	  droSendValues("GWZ",1,grblValuesIntToStrFloat(grblS11AxisValIntWpos[grblS11axisZ]));

	  droSendValues("GWCO",1,grblValuesIntToStrFloat(grblS11WcoValInt[grblS11WcoX]) + " " + grblValuesIntToStrFloat(grblS11WcoValInt[grblS11WcoY]) + " " + 
	                             grblValuesIntToStrFloat(grblS11WcoValInt[grblS11WcoZ]));
	  droSendValues("GMODE",1,grblS11PosModeStr);*/
  }

  void droStatusOtherSend()
  {
	  String modeAuto = "On";
	  if (!interParModeAuto) {modeAuto = "Off";}
	  droSendValues("GPAUTO",1,modeAuto);*/
  }

  void droSendValues(String droName, int droIndex, String droValue)
  {
	  if (interParDroConnected)
	  {
		  String droCommand = "";

		  droCommand = interParDroIdentity + "," + droName + "," + String(droIndex) + "," + droValue + "\n";
		  Serial.print(droCommand);
	  }
  }
#endif
/*------------------------------------ Monitor subroutines ------------------------------------
' You can send commands to the program by IDE monitor command line or, if connected, by the DRO
' for internal parameters or, in transit, for GRBL
'
' commRx: GRBL/xxxxx/ for GRBL command
'         INTERNAL/xxxxxx for internal command
'
' GRBL/G91 G0 X12 Y12 Z5
' GRBL/G90 G0 X0 Y0 z0
' INTERNAL/DROconnected/0     INTERNAL/GRBLPOLLPER/250
' INTERNAL/MODEAUTO/0
'----------------------------------------------------------------------------------------------
*/
void monCommTreatment(String commRx)
{
	commRx.toUpperCase();
	toSplitCommand(commRx, "/");                                              // To put received parameters in array
	String destDevice = SplittingArray[0];                                    // Destination device
	if (destDevice == "GRBL")
	{
		grblCmdSend(SplittingArray[1]);
	}
	else if (destDevice == "INTERNAL")                                        // INTERNAL/POLLstat/0
	{
		interCmdExecute(SplittingArray[1], SplittingArray[2]);
	}
	else
	{
    #ifdef PS3_debug
		  Serial.println("Dispositivo desconocido!! " + destDevice);
    #endif
	}
}
//------------------------------------ Internal subroutines
void interCmdExecute(String intCmdFct, String intCmdPar1)
{
	int cmdPar1Val = intCmdPar1.toInt();
	if (intCmdFct  == "DROCONNECTED")                                           // Connect or disconnect DRO
	{
  #ifdef DRO
		//if (cmdPar1Val == 1) {interParDroConnected = true;} else {interParDroConnected = false;}
  #endif
	}
	else if (intCmdFct  == "GRBLPOLLPER")
	{
		grblPollingPeriod = cmdPar1Val;
	}
	else if (intCmdFct  == "MODEAUTO")
	{
		if (cmdPar1Val == 1) {interParModeAuto = true;} else {interParModeAuto = false;}
	}
	else
	{
    #ifdef PS3_debug
		  Serial.println("Cmd interno " + intCmdFct + " !!");
    #endif
	}
}

//------------------------------------ Communication subroutines
void serial1Event()                                                   // GRBL serial
{
	while (Serial1.available())
	{
		char MinChar = (char)Serial1.read();                              // Char received from GRBL
		if (MinChar == '\n')
		{
			grblStringAvailable = true;
		}
		else
		{
			if (MinChar >= ' ') {grblStringRx += MinChar;}                  // >= ' ' to avoid not wanted ctrl char.
		}
	}
}

void serialEvent()                                                    // IDE monitor or DRO serial
{
	while (Serial.available())
	{
		char monChar = (char)Serial.read();                               // Char received from IDE monitor
		if (monChar == '\n')                                              // If new line char received = end of command line
		{
			monCommandNew  = true;
		}
		else
		{
			if (monChar >= ' ') {monCommand += monChar;}                    // >= ' ' to avoid not wanted ctrl char.
		}
	}
}
//---------------------------------------------- Tools section
/*-----------------------------------------------------------------------------------------------------------------------------
*' toSplitCommand for to split received string delimited with expected char separator par1, par2, par3, .....
*' and put them in an array (SplittingArray)
*'-----------------------------------------------------------------------------------------------------------------------------
*/
void toSplitCommand(String SplitText, String SplitChar) {
	SplitText = SplitChar + SplitText + SplitChar;
	int SplitIndex = -1;
	int SplitIndex2;

	for(int i = 0; i < SplittingArrayDim - 1; i++) {
		SplitIndex = SplitText.indexOf(SplitChar, SplitIndex + 1);
		SplitIndex2 = SplitText.indexOf(SplitChar, SplitIndex + 1);
		if (SplitIndex < 0 || SplitIndex2 < 0){break;}
		if(SplitIndex2 < 0) SplitIndex2 = SplitText.length() ;
		SplittingArray[i] = SplitText.substring(SplitIndex+1, SplitIndex2);
	}
}
//*******************************************************************************
#ifndef DRO
void Dualshock3_RX(){
  if (PS3.getButtonClick(TRIANGLE)) {control_ps3(1); return;}
  //if (PS3.getButtonClick(CIRCLE)) {control_ps3(2); return;}
  if (PS3.getButtonClick(CROSS)) {control_ps3(3); return;}
  if (PS3.getButtonClick(SQUARE)) {control_ps3(4); return;}
  
  if (PS3.getButtonClick(UP)) {control_ps3(5); return;}
  if (PS3.getButtonClick(RIGHT)) {control_ps3(6); return;}
  if (PS3.getButtonClick(DOWN)) {control_ps3(7); return;}
  if (PS3.getButtonClick(LEFT)) {control_ps3(8); return;}
  
  if (PS3.getButtonClick(L1)) {control_ps3(9); return;}
  if (PS3.getButtonClick(L2)) {control_ps3(10); return;}
  //if (PS3.getButtonClick(L3)) {control_ps3(11); return;}
  //if (PS3.getButtonClick(R1)) {control_ps3(12); return;}
  if (PS3.getButtonClick(R2)) {control_ps3(13); return;}
  //if (PS3.getButtonClick(R3)) {control_ps3(14); return;}

  if (PS3.getButtonClick(PS))  {control_ps3(15); return;}
  // if (PS3.getButtonClick(SELECT)) {control_ps3(16); return;}
  if (PS3.getButtonClick(START)) {control_ps3(17); return;}
// ADC joystick en posición central valor 127 (+-10)
  // joystick izquierdo
  //if (PS3.getAnalogHat(LeftHatX) > 150 || PS3.getAnalogHat(LeftHatX) <100) {control_ps3(18);} else grblCmdSend("G4P0");
  //if (PS3.getAnalogHat(LeftHatY) > 150 || PS3.getAnalogHat(LeftHatY) < 100) {control_ps3(19); return;} else grblCmdSend("G4P0");
  // joystick derecho
  if (PS3.getAnalogHat(RightHatX) > 150 || PS3.getAnalogHat(RightHatX) < 100) {control_ps3(20); return;}
  if (PS3.getAnalogHat(RightHatY) > 150 || PS3.getAnalogHat(RightHatY) < 100) {control_ps3(21); return;}
  grblCmdSend("G4P0");
}
void control_ps3 (byte boton){
bool interParModeAuto_ = interParModeAuto;
#ifdef PS3_debug
  Serial.print("Boton: ");
  Serial.println(boton);
#endif
  interParModeAuto = false;
  switch (boton){
    case 1:
      grblCmdSend("!");                                     // Pausa-> triangulo
    break;
    case 2:
    //  grblCmdSend("G38.2Z-45F100;G92Z5.0");               // Probe-> circulo
    break;
    case 3:
      grblCmdSend("$X");                                    // desbloqueo-> equis
    break;
    case 4:
      grblCmdSend("$G");                                    // estado-> cuadrado
    break;
    case 5:
      grblCmdSend("$J=G91F" + String(velocidad) + "Z5.0");  // subir 5 eje Z-> subir
    break;
    case 6:
      grblCmdSend("G91G0X5.0");                             // derecha 5 eje X-> derecha
    break;
    case 7:
      grblCmdSend("$J=G91F" + String(velocidad) + "Z-5.0"); // bajar 5 eje Z-> bajar
    break;
    case 8:
      grblCmdSend("G91G0X-5.0");                            // izquierda 5 eje X->izquierda
    break;
    case 9:
      if (velocidad <500) velocidad = velocidad +10;
    break;
    case 10:
      if (velocidad >19) velocidad = velocidad -10;
    break;    
    case 13:
      digitalWrite(grblTxSwitch, HIGH);
      delayMicroseconds(10);
      delay (50);
      Serial1.write (0x18);                                 // reset-> R2
      delay (50);
      digitalWrite(grblTxSwitch, LOW);
    break;
    case 15:
      grblCmdSend("$H");                                    // home-> PS
    break;
    //case 16:
    //  grblCmdSend("G90G0X0Y0Z0");                         // origen 0-> select
    //break;
    case 17:
      grblCmdSend("G90G0X110Y10Z10");                       // posición taladro manual -> start      
    break;
    /*case 18:
        if (PS3.getAnalogHat(LeftHatX) > 150)
          grblCmdSend("$J=G91F750X0.75");                   // joystick izquierdo eje X derecha     
        else
          grblCmdSend("$J=G91F750X-0.75");                  // joystick izquierdo eje X izquierda
    break;
    case 19:
        if (PS3.getAnalogHat(LeftHatY) > 150)
          grblCmdSend("$J=G91F7501Y-0.75");                 // joystick izquierdo eje Y retrocede   
        else
          grblCmdSend("$J=G91F750Y0.75");                   // joystick izquierdo eje Y avanza
    break;*/
    case 20:
      if (run_joystick == false){
        if (PS3.getAnalogHat(RightHatX) > 150)
          grblCmdSend("$J=G91F250X0.5");                    // joystick derecho eje X derecha     
        else
          grblCmdSend("$J=G91F250X-0.5");                   // joystick derecho eje X izquierda   
        run_joystick =true; 
        joystick_run =millis();                             // obtiene el tiempo actual 
      }  
    break;
    case 21:
      if (run_joystick == false){
        if (PS3.getAnalogHat(RightHatY) > 150)
          grblCmdSend("$J=G91F2501Y-0.5");                  // joystick derecho eje Y retrocede   
        else
          grblCmdSend("$J=G91F250Y0.5");                    // joystick derecho eje Y avanza
        run_joystick =true;
        joystick_run =millis();                             // obtiene el tiempo actual 
      }
    break;
  }
  interParModeAuto =interParModeAuto_;
}
#endif
//////////////////////////////////////////////////////////////////////////////////
void lcdClsRow(int rowNum)
{
	lcdPrint("                    ", 0, rowNum);
}
void lcdCls()
{
	lcd.clear();
}
void lcdPrint(String lcdText, int lcdCol,int lcdRow)
{
	lcd.setCursor(lcdCol,lcdRow);
	lcd.print(lcdText);
}
void lcdPrintLength(String lcdText, int lcdCol,int lcdRow, int textLength)
{
	String textToLength = lcdText + "                    ";
	textToLength = textToLength.substring(0,textLength);
	
	lcdPrint(textToLength, lcdCol, lcdRow);
}

