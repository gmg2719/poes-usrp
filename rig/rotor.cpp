/*
    HRPT-Decoder, a software for processing NOAA-POES high resolution weather satellite images.
    Copyright (C) 2009 Free Software Foundation, Inc.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Email: <postmaster@poes-weather.com>
    Web: <http://www.poes-weather.com>
*/
//---------------------------------------------------------------------------
#include <QSettings>
#include <stdlib.h>
#include <math.h>

#include "rig.h"
#include "rotor.h"
#include "utils.h"
#include "qextserialport.h"

#define SER_IO_BUFF_SIZE 128

static const double RTD = 180.0 / M_PI;
static const double DTR = M_PI / 180.0;

//---------------------------------------------------------------------------
TRotor::TRotor(TRig *_rig)
{
    rig = _rig;

    rotor_type   = RotorType_Stepper;
    serialPort   = new QextSerialPort(QextSerialPort::Polling);
    serialPort_2 = new QextSerialPort(QextSerialPort::Polling);
    iobuff       = (char *) malloc(SER_IO_BUFF_SIZE);

    stepper = new TStepper(this);
    gs232b  = new TGS232B(this);
    spid    = new TAlphaSpid(this);
    jrk     = new TJRK(this);
    monster = new TMonstrum(this);

    parkAz = 0;
    parkEl = 90;

    az_max = 360; az_min = 0;
    el_max = 90;  el_min = 0;

    commtype = Comm_Default;
    flags = 0;
}

//---------------------------------------------------------------------------
TRotor::~TRotor(void)
{
    closePort();

    delete stepper;
    delete gs232b;
    delete spid;
    delete jrk;
    delete monster;

    delete serialPort;
    delete serialPort_2;

    if(iobuff != NULL)
        free(iobuff);
}

//---------------------------------------------------------------------------
void TRotor::writeSettings(QSettings *reg)
{
    reg->beginGroup("Rotor");

      reg->setValue("Flags", (int) flags);
      reg->setValue("Type", (int) rotor_type);

      reg->setValue("CommType", commtype);
      reg->setValue("Host", host);
      reg->setValue("Port", port);

      reg->setValue("Park", parkingEnabled());
      reg->setValue("ParkAz", parkAz);
      reg->setValue("ParkEl", parkEl);

      reg->setValue("AzMax", az_max);
      reg->setValue("ElMax", el_max);
      reg->setValue("AzMin", az_min);
      reg->setValue("ElMin", el_min);

      stepper->writeSettings(reg);
      gs232b->writeSettings(reg);
      spid->writeSettings(reg);
      jrk->writeSettings(reg);
      monster->writeSettings(reg);

    reg->endGroup();
}

//---------------------------------------------------------------------------
void TRotor::readSettings(QSettings *reg)
{
    reg->beginGroup("Rotor");

      flags = reg->value("Flags", 0).toInt();
      rotor_type = (TRotorType_t) reg->value("Type", 0).toInt();

      commtype = (TCommType) reg->value("CommType", 0).toInt();
      host = reg->value("Host", "192.168.1.10").toString();
      port = reg->value("Port", 1234).toInt();

      parkingEnabled(reg->value("Park", 0).toBool());
      parkAz = reg->value("ParkAz", 0).toDouble();
      parkEl = reg->value("ParkEl", 90).toDouble();

      az_max = reg->value("AzMax", 360).toDouble();
      el_max = reg->value("ElMax", 90).toDouble();
      az_min = reg->value("AzMin", 0).toDouble();
      el_min = reg->value("ElMin", 0).toDouble();

      stepper->readSettings(reg);
      gs232b->readSettings(reg);
      spid->readSettings(reg);
      jrk->readSettings(reg);
      monster->readSettings(reg);

    reg->endGroup();
}

//---------------------------------------------------------------------------
QString TRotor::getErrorString(void)
{
    switch(rotor_type)
    {
    case RotorType_Stepper:  return stepper->errorString();
    case RotorType_GS232B:   return gs232b->errorString();
    case RotorType_SPID:     return spid->errorString();
    case RotorType_JRK:      return jrk->errorString();
    case RotorType_Monstrum: return monster->errorString();

    default:
        return "Fatal: Unknown rotor type!";
    }
}

//---------------------------------------------------------------------------
QString TRotor::getStatusString(void)
{
    if(rotor_type == RotorType_Monstrum)
        return monster->statusString();
    else
        return "Status not supported for selected type!";
}

//---------------------------------------------------------------------------
void TRotor::enable(bool enable)
{
    flags &= ~R_ROTOR_ENABLE;
    flags |= enable ? R_ROTOR_ENABLE:0;
}

//---------------------------------------------------------------------------
QString TRotor::getRotorName(void) const
{
    switch(rotor_type)
    {
    case RotorType_Stepper:  return "Stepper";
    case RotorType_GS232B:   return "Yaesu GS-232b";
    case RotorType_SPID:     return "Alfa-SPID";
    case RotorType_JRK:      return "Pololu Jrk Motor Control";
    case RotorType_Monstrum: return "Monstrum X-Y";

    default:
        return "Unknown rotor type";
    }
}

//---------------------------------------------------------------------------
void TRotor::parkingEnabled(bool park)
{
    flags &= ~R_ROTOR_PARK;
    flags |= park ? R_ROTOR_PARK:0;
}

//---------------------------------------------------------------------------
bool TRotor::openPort(void)
{
    switch(rotor_type)
    {
    case RotorType_Stepper:  return stepper->openLPT();
    case RotorType_GS232B:   return gs232b->openCOM();
    case RotorType_SPID:     return spid->openCOM();
    case RotorType_JRK:      return jrk->openCOM();
    case RotorType_Monstrum: return monster->openCOM();

    default:
        return false;
    }
}

//---------------------------------------------------------------------------
void TRotor::closePort(void)
{
    stepper->closeLPT();
    gs232b->closeCOM();
    spid->closeCOM();
    jrk->closeCOM();
    monster->closeCOM();
}

//---------------------------------------------------------------------------
bool TRotor::isPortOpen(void)
{
    switch(rotor_type)
    {
    case RotorType_Stepper:  return stepper->isLPTOpen();
    case RotorType_GS232B:   return gs232b->isCOMOpen();
    case RotorType_SPID:     return spid->isCOMOpen();
    case RotorType_JRK:      return jrk->isCOMOpen();
    case RotorType_Monstrum: return monster->isCOMOpen();

    default:
        return false;
    }
}

//---------------------------------------------------------------------------
bool TRotor::isXY(void)
{
    return rotor_type == RotorType_Monstrum ? true:false;
}

//---------------------------------------------------------------------------
void TRotor::park(void)
{
    if(parkingEnabled())
        moveTo(parkAz, parkEl);
}

//---------------------------------------------------------------------------
bool TRotor::moveToXY(double x, double y)
{
    switch(rotor_type)
    {
    case RotorType_Monstrum: return monster->moveToXY(x, y);

    default:
        return false;
    }
}

//---------------------------------------------------------------------------
bool TRotor::moveTo(double az, double el)
{
    az = ClipValue(az, az_max, 0);
    el = ClipValue(el, el_max, 0);

    switch(rotor_type)
    {
    case RotorType_Stepper:  return stepper->moveTo(az, el);
    case RotorType_GS232B:   return gs232b->moveTo(az, el);
    case RotorType_SPID:     return spid->moveTo(az, el);
    case RotorType_JRK:      return jrk->moveTo(az, el);
    case RotorType_Monstrum: return monster->moveTo(az, el);

    default:
        return false;
    }
}

//---------------------------------------------------------------------------
bool TRotor::moveToAz(double az)
{
    az = ClipValue(az, az_max, 0);

    switch(rotor_type)
    {
    case RotorType_Stepper:  return stepper->moveToAz(az);
    case RotorType_GS232B:   return gs232b->moveToAz(az);
    case RotorType_SPID:     return spid->moveToAz(az);
    case RotorType_JRK:      return jrk->moveToAz(az);
    case RotorType_Monstrum: return monster->moveToAz(az);

    default:
        return false;
    }
}

//---------------------------------------------------------------------------
bool TRotor::moveToEl(double el)
{
    el = ClipValue(el, el_max, 0);

    switch(rotor_type)
    {
    case RotorType_Stepper:  return stepper->moveToEl(el);
    case RotorType_GS232B:   return gs232b->moveToEl(el);
    case RotorType_SPID:     return spid->moveToEl(el);
    case RotorType_JRK:      return jrk->moveToEl(el);
    case RotorType_Monstrum: return monster->moveToEl(el);

    default:
        return false;
    }
}

//---------------------------------------------------------------------------
void TRotor::stopMotor(void)
{
    switch(rotor_type)
    {
    case RotorType_GS232B:   gs232b->stop(); break;
    case RotorType_SPID:     spid->stop(); break;
    case RotorType_JRK:      jrk->stop(); break;
    case RotorType_Monstrum: monster->stop(); break;

    default:
        {}
    }
}

//---------------------------------------------------------------------------
bool TRotor::readPosition(void)
{
    switch(rotor_type)
    {
    case RotorType_Stepper:  return true;
    case RotorType_GS232B:   return gs232b->readPosition();
    case RotorType_SPID:     return spid->readPosition();
    case RotorType_JRK:      return jrk->readPosition();
    case RotorType_Monstrum: return monster->readPosition();

    default:
        return false;
    }
}

//---------------------------------------------------------------------------
double TRotor::getAzimuth(void)
{
    switch(rotor_type)
    {
    case RotorType_Stepper: return stepper->current_az;
    case RotorType_GS232B: return gs232b->current_az;
    case RotorType_SPID: return spid->current_az;
    case RotorType_JRK: return jrk->current_az;
    case RotorType_Monstrum: return monster->current_x;

    default:
        return 0;
    }
}

//---------------------------------------------------------------------------
double TRotor::getElevation(void)
{
    switch(rotor_type)
    {        
    case RotorType_Stepper: return stepper->current_el;
    case RotorType_GS232B: return gs232b->current_el;
    case RotorType_SPID: return spid->current_el;
    case RotorType_JRK: return jrk->current_el;
    case RotorType_Monstrum: return monster->current_y;

    default:
        return 0;
    }
}

//---------------------------------------------------------------------------
void TRotor::setAzimuth(double az)
{
    if(rotor_type == RotorType_Stepper)
       stepper->current_az = az;
}

//---------------------------------------------------------------------------
void TRotor::setElevation(double el)
{
    if(rotor_type == RotorType_Stepper)
       stepper->current_el = el;
}

//---------------------------------------------------------------------------
unsigned long TRotor::getRotationTime(double toAz, double toEl)
{
 unsigned long ms;

    switch(rotor_type)
    {
    case RotorType_GS232B:
       ms = gs232b->getRotationTime(toAz, toEl);
    break;

    case RotorType_SPID:
       ms = spid->getRotationTime(toAz, toEl);
    break;

    default:
        return ms = 0;
    }

    return ms;
}

//---------------------------------------------------------------------------
void TRotor::AzEltoXY(double az, double el, double *x, double *y)
{
    double accuracy = 0.01; // rotor accuracy in degrees

    if(el <= accuracy)
        *x = 90.0;
    else if(el >= (90.0 - accuracy))
        *x = 0.0;
    else
        *x = 90.0 - atan(-cos(az * DTR) / tan(el * DTR)) * RTD;

    *y = 90.0 - asin(sin(az * DTR) * cos(el * DTR)) * RTD;
}

//---------------------------------------------------------------------------
void TRotor::XYtoAzEl(double X, double Y, double *az, double *el)
{
    // TODO
    X = X;
    Y = Y;
    *az = *az;
    *el = *el;
}
