/*******************************************************************************
  Copyright(c) 2016 Magnus W. Eriksen. All rights reserved.

  Arduino Cap, Driver for DIY servo driven dustcaps.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU Library General Public License
  along with this library; see the file COPYING.LIB.  If not, write to
  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  Boston, MA 02110-1301, USA.

  The full GNU General Public License is included in this distribution in the
  file called LICENSE.
*******************************************************************************/
#include "arduino_cap.h"
#include <memory>
#include <string>

#define CALIB_TAB "Calibrate"

// We declare an auto pointer to ArduinoCap.
std::unique_ptr<ArduinoCap> arduino_cap(new ArduinoCap());

void ISGetProperties(const char *dev)
{
    arduino_cap->ISGetProperties(dev);
}

void ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int num)
{
    arduino_cap->ISNewSwitch(dev, name, states, names, num);
}

void ISNewText(const char *dev, const char *name, char *texts[], char *names[], int num)
{
    arduino_cap->ISNewText(dev, name, texts, names, num);
}

void ISNewNumber(const char *dev, const char *name, double values[], char *names[], int num)
{
    arduino_cap->ISNewNumber(dev, name, values, names, num);
}

void ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}
void ISSnoopDevice(XMLEle *root)
{
    arduino_cap->ISSnoopDevice(root);
}

ArduinoCap::ArduinoCap() : LightBoxInterface(this, false)
{
    setVersion(0,1);
    // Initialize all vars for predictable behavior.
    isConnecting = true;
    isMoving = false;
    isMoveStep = false;
    isClosing = false;
    moveToABS = 0;
}

ArduinoCap::~ArduinoCap()
{

}

bool ArduinoCap::initProperties()
{
    INDI::DefaultDevice::initProperties();

    /************************************************************************************
    * Main Tab
    * ***********************************************************************************/
    IUFillNumber(&MoveSteppN[0], "STEPP_ABS", "stepp to (degree)", "%6.0f", 0, 180, 1, 0);
    IUFillNumberVector(&MoveSteppNP, MoveSteppN, 1, getDeviceName(), "STEPP_MOVE", "Stepp",
            MAIN_CONTROL_TAB, IP_RW, 60, IPS_IDLE);

    IUFillNumber(&AbsolutePosN[0], "ABS_POS", "abs position (degree)", "%6.2f", 0, 180, 1, 0);
    IUFillNumberVector(&AbsolutePosNP, AbsolutePosN, 1, getDeviceName(), "ABSOLUTE_POSITION",
            "Servo position", MAIN_CONTROL_TAB, IP_RO, 60, IPS_IDLE);

    initDustCapProperties(getDeviceName(), MAIN_CONTROL_TAB);
    initLightBoxProperties(getDeviceName(), MAIN_CONTROL_TAB);

    /************************************************************************************
    * Options Tab
    * ***********************************************************************************/
    IUFillSwitch(&LightTypeS[0], "TYPE_USBRELAY2", "use USBRelay2", ISS_ON);
    IUFillSwitch(&LightTypeS[1], "TYPE_NONE", "do not use lightsource", ISS_OFF);
    IUFillSwitchVector(&LightTypeSP, LightTypeS, 2, getDeviceName(), "TYPE_SELECT",
            "Light device", OPTIONS_TAB, IP_RW, ISR_1OFMANY, 0, IPS_OK);

    IUFillText(&DevicePathT[0],"DEVICE_PATH","usb device path","/dev/ttyUSB0");
    IUFillTextVector(&DevicePathTP,DevicePathT,1,getDeviceName(),"DEVICE_PATH","USB device path",
            OPTIONS_TAB,IP_RW,60,IPS_IDLE);

    /************************************************************************************
    * Calibration Tab
    * ***********************************************************************************/
    IUFillNumber(&ServoIDN[0], "SERVO_ID", "set servo id (0-7)", "%6.0f", 0, 6, 1, 0);
    IUFillNumberVector(&ServoIDNP, ServoIDN, 1, getDeviceName(), "SERVO_ID",
            "Servo ID", CALIB_TAB, IP_RW, 60, IPS_OK);

    IUFillNumber(&LightSwitchN[0], "LIGHT_SWITCH", "set flat light device", "%6.0f", 0, 7, 1, 0);
    IUFillNumberVector(&LightSwitchNP, LightSwitchN, 1, getDeviceName(), "LIGHT_SWITCH",
            "Light switch dev nbr", CALIB_TAB, IP_RW, 60, IPS_OK);

    IUFillNumber(&ServoTravelN[0], "LIMIT_OPEN", "set open travel (degrees)", "%6.0f", 0, 180, 1, 140);
    IUFillNumber(&ServoTravelN[1], "LIMIT_CLOSE", "set close travel (degrees)", "%6.0f", 0, 180, 1, 40);
    IUFillNumberVector(&ServoTravelNP, ServoTravelN, 2, getDeviceName(), "ROOF_TRAVEL_LIMITS",
            "Max travel Limits", CALIB_TAB, IP_RW, 60, IPS_OK);

    IUFillNumber(&ServoLimitN[0], "LIMIT_OPEN", "set open limit (%)", "%6.0f", 50, 100, 1, 100);
    IUFillNumber(&ServoLimitN[1], "LIMIT_CLOSE", "set close limit (%)", "%6.0f", 50, 100, 1, 100);
    IUFillNumberVector(&ServoLimitNP, ServoLimitN, 2, getDeviceName(), "ROOF_PREFERED_LIMITS",
            "Prefered Limits", CALIB_TAB, IP_RW, 60, IPS_OK);

    setDriverInterface(AUX_INTERFACE | DUSTCAP_INTERFACE | LIGHTBOX_INTERFACE);
    addDebugControl();
    addSimulationControl();

    return true;
}

void ArduinoCap::SetupParams()
{
    // Initialize the Parkdata class
    if (parkData.InitPark())
    {
        IUResetSwitch(&ParkCapSP);
        // Check park status and update ParkCapSP
        if (parkData.isParked())
        {
            DEBUG(INDI::Logger::DBG_SESSION, "Parkstatus initialized to parked");
            ParkCapS[0].s = ISS_ON;
            ParkCapSP.s = IPS_OK;
            IDSetSwitch(&ParkCapSP, NULL);
            isClosing = true;
        } 
        else
        {
            DEBUG(INDI::Logger::DBG_SESSION, "Parkstatus initialized to unparked");
            ParkCapS[1].s = ISS_ON;
            ParkCapSP.s = IPS_OK;
            IDSetSwitch(&ParkCapSP, NULL);
            isClosing = false;
        }
    }
}

void ArduinoCap::ISGetProperties(const char *dev)
{
    INDI::DefaultDevice::ISGetProperties(dev);
}

bool ArduinoCap::updateProperties()
{
    INDI::DefaultDevice::updateProperties();

    if (isConnected())
    {
        // Main tab
        defineProperty(&ParkCapSP);
        defineProperty(&MoveSteppNP);
        defineProperty(&AbsolutePosNP);
        
        // Options tab
        defineProperty(&LightTypeSP);
        defineProperty(&DevicePathTP);

        // Calib tab
        defineProperty(&ServoIDNP);
        defineProperty(&LightSwitchNP);
        defineProperty(&ServoTravelNP);
        defineProperty(&ServoLimitNP);
    }
    else
    {
        // Main tab
        deleteProperty(ParkCapSP.name);
        deleteProperty(LightSP.name);
        deleteProperty(MoveSteppNP.name);
        deleteProperty(AbsolutePosNP.name);

        // Options tab
        deleteProperty(LightTypeSP.name);
        deleteProperty(DevicePathTP.name);

        // Calib tab
        deleteProperty(ServoIDNP.name);
        deleteProperty(LightSwitchNP.name);
        deleteProperty(ServoTravelNP.name);
        deleteProperty(ServoLimitNP.name);
    }

    return true;
}


const char * ArduinoCap::getDefaultName()
{
    return (char *)"Arduino Cap";
}

bool ArduinoCap::Connect()
{
    std::string cmd = "python3 -c 'import pyfirmata';echo $?";
    int res = PopenInt(cmd.c_str());
    DEBUGF(INDI::Logger::DBG_DEBUG, "*****Connect res = %d", res);

    if (INDI::DefaultDevice::isSimulation())
        DEBUGF(INDI::Logger::DBG_SESSION, "%s is online (simulated)", getDeviceName());
    else if (res != 0) 
    {
        DEBUGF(INDI::Logger::DBG_ERROR, "%s is offline, did not initialize. Python library pyfirmata not found", getDeviceName());
        return false;
    } 
    else
        DEBUGF(INDI::Logger::DBG_SESSION, "%s is online.", getDeviceName());

    SetupParams();
    SetTimer(350);
    return true;
}

bool ArduinoCap::Disconnect()
{
    DEBUGF(INDI::Logger::DBG_SESSION, "%s is offline.", getDeviceName());

    return true;
}

bool ArduinoCap::ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if (processLightBoxNumber(dev, name, values, names, n))
            return true;

        // Move in steps, useful when calibrating limits.
        if (strcmp(name, MoveSteppNP.name)==0)
        {
            if (values[0] >= 0 && values[0] <= 180) {
                IUUpdateNumber(&MoveSteppNP, values, names, n);
                MoveSteppNP.s = IPS_OK;
                IDSetNumber(&MoveSteppNP, NULL);

                return MoveToABS(values[0]);
            }
        }
        // Update all remainig switches.
        INumberVectorProperty *updateNP = NULL;
        if (strcmp(name, ServoIDNP.name)==0)
        {
            updateNP = &ServoIDNP;
        } 
        else if (strcmp(name, LightSwitchNP.name)==0)
        {
            updateNP = &LightSwitchNP;
        }
        else if (strcmp(name, ServoTravelNP.name)==0)
        {
            updateNP = &ServoTravelNP;
        }
        else if (strcmp(name, ServoLimitNP.name)==0)
        {
            updateNP = &ServoLimitNP;
        }

        if (updateNP != NULL)
        {
            IUUpdateNumber(updateNP, values, names, n);
            updateNP->s = IPS_OK;
            IDSetNumber(updateNP, NULL);
            DEBUGF(INDI::Logger::DBG_DEBUG, "*****SetNumber->name == %s", updateNP->name);
            return true;
        }

    }

    return INDI::DefaultDevice::ISNewNumber(dev, name, values, names, n);
}

bool ArduinoCap::ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if (processLightBoxText(dev, name, texts, names, n))
            return true;

        if (strcmp(name,DevicePathTP.name)==0)
        {
            IUUpdateText(&DevicePathTP, texts, names, n);
            DevicePathTP.s = IPS_OK;
            IDSetText(&DevicePathTP, NULL);
            return true;
        }
    }
    return INDI::DefaultDevice::ISNewText(dev, name, texts, names, n);
}

bool ArduinoCap::ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n)
{
    if(strcmp(dev,getDeviceName())==0)
    {
        if (processLightBoxSwitch(dev, name, states, names, n))
            return true;

        if (processDustCapSwitch(dev, name, states, names, n))
            return true;

        // Update if we are using USBRelay2, or WiringPi as light source.
        if (strcmp(name, LightTypeSP.name)==0)
        {
            bool isUsbRelay2 = strcmp(names[0], "TYPE_USBRELAY2") == 0;
            IUUpdateSwitch(&LightTypeSP, states, names, n);
            LightTypeSP.s = IPS_OK;
            IDSetSwitch(&LightTypeSP, NULL);
            DEBUGF(INDI::Logger::DBG_DEBUG, "Light switch type set to %s"
                    , isUsbRelay2 ? "USBRelay2 Roof." : "No lightsource");

            if (isUsbRelay2 && !isConnecting)
                defineProperty(&LightSP);
            else
                deleteProperty(LightSP.name);
            return true;
        }
    }
    return INDI::DefaultDevice::ISNewSwitch(dev, name, states, names, n);
}

bool ArduinoCap::ISSnoopDevice(XMLEle *root)
{
    return INDI::DefaultDevice::ISSnoopDevice(root);
}

bool ArduinoCap::saveConfigItems(FILE *fp)
{
    IUSaveConfigNumber(fp, &ServoIDNP);
    IUSaveConfigNumber(fp, &LightSwitchNP);
    IUSaveConfigSwitch(fp, &LightTypeSP);
    IUSaveConfigText(fp, &DevicePathTP);
    IUSaveConfigNumber(fp, &ServoTravelNP);
    IUSaveConfigNumber(fp, &ServoLimitNP);
    return true;
}

IPState ArduinoCap::ParkCap()
{    
    double absAtStart = AbsolutePosN[0].value;
    isClosing = true;

    IUResetSwitch(&ParkCapSP);
    ParkCapS[0].s = ISS_ON;
    ParkCapSP.s = IPS_BUSY;
    IDSetSwitch(&ParkCapSP, NULL);

    if (!isConnecting && Move())
        return IPS_OK;
    return IPS_BUSY;
}

IPState ArduinoCap::UnParkCap()
{
    double absAtStart = AbsolutePosN[0].value;
    isClosing = false;

    IUResetSwitch(&ParkCapSP);
    ParkCapS[1].s = ISS_ON;
    ParkCapSP.s = IPS_BUSY;
    IDSetSwitch(&ParkCapSP, NULL);

    if (!isConnecting && Move())
        return IPS_OK;
    return IPS_BUSY;
}

void ArduinoCap::SetOKParkStatus()
{
    // Update park status after TimerHit has completed.
    IUResetSwitch(&ParkCapSP);
    if (isClosing)
        ParkCapS[0].s = ISS_ON;
    else
        ParkCapS[1].s = ISS_ON;
    ParkCapSP.s = IPS_OK;
    IDSetSwitch(&ParkCapSP, NULL);

    parkData.SetParked(isClosing);
}

bool ArduinoCap::Move()
{
    // Initialize limit, steps and start TimerHit to prak or unpark cap.
    double absAtStart = AbsolutePosN[0].value;
    moveToABS = getFullABS(isClosing);
    isMoving = true;

    // No need to have external light on when closing cap.
    if (isClosing)
    {
        ISState *states;
        ISState s[2] = {ISS_OFF, ISS_ON};
        states = s;
        char *names[2] = {strdup("FLAT_LIGHT_ON"), strdup("FLAT_LIGHT_OFF")};
        ISNewSwitch(getDeviceName(), LightSP.name, states, names, 2);
    }

    DEBUGF(INDI::Logger::DBG_SESSION, "%s from %6.2f, to %6.2f"
            , isClosing ? "Closing" : "Opening", absAtStart, moveToABS);
    return DoMove();
}

bool ArduinoCap::MoveToABS(double moveTo)
{
    moveToABS = moveTo;
    isMoving = true;
    isMoveStep = true;

    DEBUGF(INDI::Logger::DBG_SESSION, "Setting servo to %6.2f wihout interpolation"
            , moveToABS);
    return DoMove();
}

bool ArduinoCap::DoMove()
{
    // DoMove. Used by park / unpark, and move step open / close.
    if (isMoving)
    {
        double servoIs = AbsolutePosN[0].value;
        std::string usbComDev = DevicePathT[0].text; 
        double servoID = ServoIDN[0].value;
        if (!isMoveStep)
        {
            // Servo move cmd string
            std::string cmd = "/usr/bin/arduino_servo.py "
                + usbComDev 
                + " "
                + std::to_string(static_cast<int>(servoID))
                + " " 
                + std::to_string(static_cast<int>(moveToABS))
                + " "
                + std::to_string(static_cast<int>(servoIs))
                + ";echo $?";
            if (!INDI::DefaultDevice::isSimulation())
            {
                int res = PopenInt(cmd.c_str());
                if (res != 0)
                {
                    DEBUGF(INDI::Logger::DBG_ERROR, "Moving servo failed. Attempted cmd was: %s", cmd.c_str());
                    return false;
                }
                else 
                {
                    DEBUGF(INDI::Logger::DBG_DEBUG, "*****Moving servo to interpolated with CMD: %s", cmd.c_str());
                    setABS(moveToABS);
                }
            }
            isMoving = false;
        }
        else
        {
            // Servo move cmd string
            std::string cmd = "/usr/bin/arduino_servo.py "
                + usbComDev 
                + " "
                + std::to_string(static_cast<int>(servoID))
                + " " 
                + std::to_string(static_cast<int>(moveToABS))
                + " "
                + std::to_string(static_cast<int>(moveToABS))
                + ";echo $?";
            if (!INDI::DefaultDevice::isSimulation())
            {
                int res = PopenInt(cmd.c_str());
                if (res != 0)
                {
                     DEBUGF(INDI::Logger::DBG_ERROR, "Moving servo failed. Attempted cmd was: %s", cmd.c_str());
                }
                else 
                {
                    DEBUGF(INDI::Logger::DBG_DEBUG, "*****Moving servo to ABS with CMD: %s", cmd.c_str());
                    setABS(moveToABS);
                }
            }
            isMoving = false;
        }

        if (!isMoving && !isMoveStep)
            SetOKParkStatus();
        if (!isMoving)
            isMoveStep = false;
    }
    return true;
}

bool ArduinoCap::EnableLightBox(bool enable)
{   
    int powerSwitch = LightSwitchN[0].value;
    bool usbrelay2 = LightTypeS[0].s == ISS_ON ? true : false;
    
    if(!usbrelay2)
        return true;

    // indi_setprop returns nothing with popen on fail or success.
    // Use indi_getprop to test that we can read the desired property. ie. it exists.
    std::string check_cmd;
    check_cmd = "indi_getprop \"USBRelay2 Roof.POWER_SWITCH_" + std::to_string(powerSwitch) + ".POWER_ON_SWITCH\"";
    
    int ret = PopenInt(check_cmd.c_str());

    if(ret != 0)
    {
        DEBUGF(INDI::Logger::DBG_DEBUG, "%s flat light failed. Attempted test cmd was: %s", enable ? "Enable" : "Disable", check_cmd.c_str());
        DEBUGF(INDI::Logger::DBG_ERROR, "%s flat light failed. Is %s %d defined?"
                , enable ? "Enable" : "Disable", "USBRelay2 Roof connected, and Power Switch", powerSwitch);

        return false;
    }

    // Use indi_setprop to turn on or off lightsource. 
    std::string cmd;
    if (enable)
        cmd = "indi_setprop \"USBRelay2 Roof.POWER_SWITCH_" + std::to_string(powerSwitch) + ".POWER_ON_SWITCH=On\"";
    else
        cmd = "indi_setprop \"USBRelay2 Roof.POWER_SWITCH_" + std::to_string(powerSwitch) + ".POWER_OFF_SWITCH=On\"";

    PopenInt(cmd.c_str());

    DEBUGF(INDI::Logger::DBG_SESSION, "Light source %s.", enable ? "enabled" : "disabled");
    return true;
}
 
bool ArduinoCap::SetLightBoxBrightness(uint16_t value)
{
    return false;
}

void ArduinoCap::TimerHit()
{
    if (isConnected() == false)
        return;

    // Set the abs position after we are sure all properties are loaded.
    if (isConnecting)
    {
        setABS(getFullABS(isClosing));
        if (LightTypeS[0].s == ISS_ON)
            defineProperty(&LightSP);
        
        isConnecting = false;
        return;
    }
}

double ArduinoCap::getFullABS(bool closed)
{
    // Get the fully open or fully closed position for our calibration.
    // No chance of reading physical servo position, so abs position is always calculated.
    double pos;
    double first = ServoTravelN[0].value;
    double second = ServoTravelN[1].value;

    if (!closed)
        if (first > second)
            pos = ServoTravelN[0].value * (ServoLimitN[0].value / 100);
        else
            pos = ServoTravelN[0].value * (1 + (100 - ServoLimitN[0].value) / 100);
    else
        if (first > second)
            pos = ServoTravelN[1].value * (1 + (100 - ServoLimitN[1].value) / 100);
        else
            pos = ServoTravelN[1].value * (ServoLimitN[1].value / 100);

    DEBUGF(INDI::Logger::DBG_DEBUG, "*****getFullABS, closed %s, pos %6.2f"
            , closed ? "true" : "false", pos);
    return pos;
}

void ArduinoCap::setABS(double pos)
{
    // Update the abs position when moving or initializing on connect.
    AbsolutePosN[0].value = pos;
    AbsolutePosNP.s = IPS_OK;
    IDSetNumber(&AbsolutePosNP, NULL);
}

int ArduinoCap::PopenInt(const char* cmd)
{
    // popen to issue system calls with return.
    FILE *fp;
    char buffer[512];

    if (!(fp = popen(cmd,"r")))
        return 1;
    
    std::string str = "";
    while (fgets(buffer, sizeof(buffer), fp) != NULL)
        str += buffer;
    pclose(fp);

    DEBUGF(INDI::Logger::DBG_DEBUG, "*****PopenString: %s", str.c_str());
    return str.empty() || str.front() == '1' ? 1 : 0;
}
