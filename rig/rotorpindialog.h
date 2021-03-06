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
#ifndef ROTORPINDIALOG_H
#define ROTORPINDIALOG_H

#include <QtGui/QDialog>

namespace Ui {
    class RotorPinDialog;
}

//---------------------------------------------------------------------------
class RotorPinDialog : public QDialog {
    Q_OBJECT
public:
    RotorPinDialog(QWidget *parent = 0);
    ~RotorPinDialog();

    void setPin(bool az, int pin1, int pin2, bool cc);

    int  getAzElPinIndex(void);
    int  getDirPinIndex(void);
    bool getCC(void);

protected:
    void changeEvent(QEvent *e);

private:
    Ui::RotorPinDialog *ui;
};

#endif // ROTORPINDIALOG_H
