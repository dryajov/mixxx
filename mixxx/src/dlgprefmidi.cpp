/***************************************************************************
                          dlgprefmidi.cpp  -  description
                             -------------------
    begin                : Thu Apr 17 2003
    copyright            : (C) 2003 by Tue & Ken Haste Andersen
    email                : haste@diku.dk
***************************************************************************/

/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

#include "dlgprefmidi.h"
#include "midiobject.h"
#include "powermate.h"
#include "mouse.h"
#include "rotary.h"
#include <qcombobox.h>
#include <QtDebug>
#include <qlabel.h>
#include <qcheckbox.h>
#include <qpushbutton.h>
#include <q3groupbox.h>
#include <q3progressdialog.h>
#include <qmessagebox.h>
#include <qtimer.h>
#include <qwidget.h>
#include "controlobject.h"
#include "midiobjectnull.h"
#include "midiledhandler.h"
#include "wwidget.h"

#ifdef __C_METRICS__
#include <cmetrics.h>
#include "defs_mixxxcmetrics.h"
#endif

#ifdef __ALSAMIDI__
  #include "midiobjectalsa.h"
#endif

#ifdef __ALSASEQMIDI__
  #include "midiobjectalsaseq.h"
#endif

#ifdef __PORTMIDI__
  #include "midiobjectportmidi.h"
#endif

#ifdef __COREMIDI__
  #include "midiobjectcoremidi.h"
#endif

#ifdef __OSSMIDI__
  #include "midiobjectoss.h"
#endif

#ifdef __WINMIDI__
  #include "midiobjectwin.h"
#endif
#ifdef __LINUX__
#include "mouselinux.h"
#endif
#ifdef __LINUX__
#include "powermatelinux.h"
#endif
#ifdef __WIN32__
#include "powermatewin.h"
#endif

#include "hercules.h"
#ifdef __LINUX__
#include "herculeslinux.h"
#endif

/*

NOTE ABOUT MOUSE SUPPORT:
There's a load of commented out stuff here from the mouse support which is slowly getting more
and more deprecated. At some point this needs to be cleaned up properly... - Adam

 */

DlgPrefMidi::DlgPrefMidi(QWidget * parent, ConfigObject<ConfigValue> * pConfig) :  QWidget(parent), Ui::DlgPrefMidiDlg()
{
    m_pConfig = pConfig;
    m_pProgressDialog = 0;
    m_pTimer = 0;
	m_pHercules = 0;

    setupUi(this);

    // Open midi
    m_pMidi = 0;
#ifdef __ALSAMIDI__
    m_pMidi = new MidiObjectALSA(m_pConfig->getValueString(ConfigKey("[Midi]","Device")));
#endif
#ifdef __ALSASEQMIDI__
    m_pMidi = new MidiObjectALSASeq(m_pConfig->getValueString(ConfigKey("[Midi]","Device")));
#endif
#ifdef __PORTMIDI__
    m_pMidi = new MidiObjectPortMidi(m_pConfig->getValueString(ConfigKey("[Midi]","Device")));
#endif
#ifdef __COREMIDI__
    m_pMidi = new MidiObjectCoreMidi(m_pConfig->getValueString(ConfigKey("[Midi]","Device")));
#endif
#ifdef __OSSMIDI__
    m_pMidi = new MidiObjectOSS(m_pConfig->getValueString(ConfigKey("[Midi]","Device")));
#endif
#ifdef __WINMIDI__
    m_pMidi = new MidiObjectWin(m_pConfig->getValueString(ConfigKey("[Midi]","Device")));
#endif

    if (m_pMidi == 0)
        m_pMidi = new MidiObjectNull(m_pConfig->getValueString(ConfigKey("[Midi]","Device")));

    // Get list of available midi configurations, and read the default configuration. If no default
    // is given, use the first configuration found in the config directory.
    QString qConfigPath = m_pConfig->getConfigPath();
    QStringList * midiConfigList = m_pMidi->getConfigList(qConfigPath.append("midi/"));
    m_pMidiConfig = 0;
    QString mappingfile = m_pConfig->getValueString(ConfigKey("[Midi]", "File"));

#ifdef __C_METRICS__
    QByteArray baMIDIMapping = m_pConfig->getValueString(ConfigKey("[Midi]","File")).toUtf8();
    cm_writemsg_utf8(MIXXXCMETRICS_MIDI_MAPPING, baMIDIMapping.data());
#endif

    bool foundmap = false;
    for (QStringList::Iterator it = midiConfigList->begin(); it != midiConfigList->end(); ++it ) {
        if (*it == mappingfile) {
            foundmap = true;
            break;
        }
    }

	// Test for migration from 1.5 to 1.6, feel free to remove this in the distant future - Adam
	if (mappingfile.endsWith(".cfg", Qt::CaseInsensitive)) {
		mappingfile = "";
	}

    if (!foundmap && !mappingfile.isEmpty()) {
        if (midiConfigList->empty())
        {
            qDebug() << "No MIDI mapping files found";
            m_pConfig->set(ConfigKey("[Midi]","File"), ConfigValue(""));
            mappingfile = "";
        }
        else
        {
            QString notfound = mappingfile;
#ifndef QT3_SUPPORT
            mappingfile = (*midiConfigList->at(0));
#else
            mappingfile = midiConfigList->at(0);
#endif

            qWarning() << "Requested MIDI mapping file" << notfound << "not found; defaulting to" << mappingfile;
            m_pConfig->set(ConfigKey("[Midi]","File"), ConfigValue(mappingfile));

            // setupMappings(m_pConfig->getValueString(ConfigKey("[Config]","Path")).append("midi/").append(m_pConfig->getValueString(ConfigKey("[Midi]","File"))));
            // m_pMidi->setMidiConfig(m_pMidiConfig);
        }
    }

    // Store default midi device
    m_pConfig->set(ConfigKey("[Midi]","Device"), ConfigValue(m_pMidi->getOpenDevice()));

    // Try initializing PowerMates
    m_pPowerMate1 = 0;
    m_pPowerMate2 = 0;
#ifdef __LINUX__
    m_pPowerMate1 = new PowerMateLinux();
    m_pPowerMate2 = new PowerMateLinux();
#endif
#ifdef __WIN32__
//    m_pPowerMate1 = new PowerMateWin();
//    m_pPowerMate2 = new PowerMateWin();
#endif
    if (m_pPowerMate1)
    {
        if (!m_pPowerMate1->opendev())
//             m_pPowerMate1->selectMapping(ComboBoxPowerMate1->currentText());
//         else
        {
            delete m_pPowerMate1;
            m_pPowerMate1 = 0;
        }
    }
    if (m_pPowerMate2)
    {
        if (!m_pPowerMate2->opendev())
//             m_pPowerMate2->selectMapping(ComboBoxPowerMate2->currentText());
//         else
        {
            delete m_pPowerMate2;
            m_pPowerMate2 = 0;
        }
    }

    // Try initializing Hercules DJ Console

#ifdef __LINUX__
    m_pHercules = new HerculesLinux();
#endif
    qDebug() << "m_pHercules init: " << m_pHercules;
    if (m_pHercules)
    {
        if (!m_pHercules->opendev())
        {
            delete m_pHercules;
            m_pHercules = 0;
        }
    }

    // Used in mouse calibration
    m_pProgressDialog = new Q3ProgressDialog("Calibrating mouse turntable", "Cancel", 30, this, "Progress...", TRUE );
    m_pProgressDialog->setMinimumDuration(0);
    m_pTimer = new QTimer(this);
    connect(m_pTimer, SIGNAL(timeout()), this, SLOT(slotUpdateProgressBar()));
    connect(m_pProgressDialog, SIGNAL(canceled()), this, SLOT(slotCancelCalibrate()));

//#ifndef __LINUX__
//    groupBoxMice->setEnabled(false);
//#endif

//    connect(ComboBoxHercules, SIGNAL(activated(int)), this, SLOT(slotApply()));
//    connect(ComboBoxMouseDevice1, SIGNAL(activated(int)), this, SLOT(slotApply()));
//    connect(ComboBoxMouseDevice2, SIGNAL(activated(int)), this, SLOT(slotApply()));
//    connect(pushButtonMouseCalibrate1, SIGNAL(clicked()), this, SLOT(slotMouseCalibrate1()));
//    connect(pushButtonMouseCalibrate2, SIGNAL(clicked()), this, SLOT(slotMouseCalibrate2()));
//    connect(pushButtonMouseHelp, SIGNAL(clicked()), this, SLOT(slotMouseHelp()));
    slotUpdate();
    slotApply();
}

DlgPrefMidi::~DlgPrefMidi()
{
    if (m_pPowerMate1)
        delete m_pPowerMate1;
    if (m_pPowerMate2)
        delete m_pPowerMate2;
    if (m_pHercules)
        delete m_pHercules;
}

void DlgPrefMidi::slotUpdate()
{
//    qDebug() << "upd";

    // Midi configurations
    ComboBoxMidiconf->clear();
    QStringList * midiConfigList = m_pMidi->getConfigList(m_pConfig->getValueString(ConfigKey("[Config]","Path")).append("midi/"));
    int j=0;
    if (midiConfigList->count()>0)
    {
        for (QStringList::Iterator it = midiConfigList->begin(); it != midiConfigList->end(); ++it )
        {
            // Insert the file name into the list, with ending (.midi.xml) stripped
            ComboBoxMidiconf->insertItem((*it).left((*it).length()-9));

            if ((*it) == m_pConfig->getValueString(ConfigKey("[Midi]","File")))
                ComboBoxMidiconf->setCurrentItem(j);
            j++;
        }
    }

    // Midi devices
    ComboBoxMididevice->clear();
    QStringList * midiDeviceList = m_pMidi->getDeviceList();
    j=0;
    QStringList::Iterator it;
    for (it = midiDeviceList->begin(); it != midiDeviceList->end(); ++it )
    {
        ComboBoxMididevice->insertItem(*it);
        if ((*it) == m_pMidi->getOpenDevice())
            ComboBoxMididevice->setCurrentItem(j);
        j++;
    }


    // PowerMates
    QStringList qPowerMateConfigList = PowerMate::getMappings();

    if (m_pPowerMate1 || m_pPowerMate2)
    {
        // Powermate 1
        if (m_pPowerMate1)
        {
            ComboBoxPowerMate1->clear();
            j=0;
            for (it = qPowerMateConfigList.begin(); it != qPowerMateConfigList.end(); ++it)
            {
                ComboBoxPowerMate1->insertItem(*it);
                if ((*it) == m_pConfig->getValueString(ConfigKey("[Controls]","PowerMateFunction1")))
                    ComboBoxPowerMate1->setCurrentItem(j);
                j++;
            }
        }
        else
        {
            TextLabelPowerMate1->setEnabled(false);
            ComboBoxPowerMate1->setEnabled(false);
        }

        // Powermate 2
        if (m_pPowerMate2)
        {
            ComboBoxPowerMate2->clear();
            j=0;
            for (it = qPowerMateConfigList.begin(); it != qPowerMateConfigList.end(); ++it)
            {
                ComboBoxPowerMate2->insertItem(*it);
                if ((*it) == m_pConfig->getValueString(ConfigKey("[Controls]","PowerMateFunction2")))
                    ComboBoxPowerMate2->setCurrentItem(j);
                j++;
            }
        }
        else
        {
            TextLabelPowerMate2->setEnabled(false);
            ComboBoxPowerMate2->setEnabled(false);
        }
    }
    else
        groupPowerMates->setEnabled(false);

    // Hercules
/*
    if (m_pHercules)
    {
        ComboBoxHercules->clear();
        j=0;
        QStringList qHerculesMappingList = Hercules::getMappings();
        for (it = qHerculesMappingList.begin(); it != qHerculesMappingList.end(); ++it)
        {
            ComboBoxHercules->insertItem(*it);
            if ((*it) == m_pConfig->getValueString(ConfigKey("[Controls]","HerculesMapping")))
                ComboBoxHercules->setCurrentItem(j);
            j++;
        }
    }
*/
/*    // Mouse
    QStringList qMouseConfigList;
    qMouseConfigList = Mouse::getMappings();

    QStringList qMouseDeviceList = Mouse::getDeviceList();
//    ComboBoxMouseDevice1->clear();
    j=0;
    for (it = qMouseDeviceList.begin(); it != qMouseDeviceList.end(); ++it)
    {
//        ComboBoxMouseDevice1->insertItem(*it);
        if ((*it) == m_pConfig->getValueString(ConfigKey("[Controls]","MouseDevice1")))
//            ComboBoxMouseDevice1->setCurrentItem(j);
        j++;
    }
//    ComboBoxMouseFunction1->clear();
//    qDebug() << "cur item " << ComboBoxMouseDevice1->currentItem();
//    if (ComboBoxMouseDevice1->currentItem()>0)
    {
//        ComboBoxMouseFunction1->setEnabled(true);
        pushButtonMouseCalibrate1->setEnabled(true);
        j=0;
        for (QStringList::Iterator it = qMouseConfigList.begin(); it != qMouseConfigList.end(); ++it)
        {
            ComboBoxMouseFunction1->insertItem(*it);
            if ((*it) == m_pConfig->getValueString(ConfigKey("[Controls]","MouseFunction1")))
                ComboBoxMouseFunction1->setCurrentItem(j);
            j++;
        }
    }
    else
    {
        ComboBoxMouseFunction1->setEnabled(false);
        pushButtonMouseCalibrate1->setEnabled(false);
    }

    ComboBoxMouseDevice2->clear();
    j=0;
    for (it = qMouseDeviceList.begin(); it != qMouseDeviceList.end(); ++it)
    {
        ComboBoxMouseDevice2->insertItem(*it);
        if ((*it) == m_pConfig->getValueString(ConfigKey("[Controls]","MouseDevice2")))
            ComboBoxMouseDevice2->setCurrentItem(j);
        j++;
    }
    ComboBoxMouseFunction2->clear();
    if (ComboBoxMouseDevice2->currentItem()>0)
    {
        ComboBoxMouseFunction2->setEnabled(true);
        pushButtonMouseCalibrate2->setEnabled(true);
        j=0;
        for (it = qMouseConfigList.begin(); it != qMouseConfigList.end(); ++it)
        {
            ComboBoxMouseFunction2->insertItem(*it);
            if ((*it) == m_pConfig->getValueString(ConfigKey("[Controls]","MouseFunction2")))
                ComboBoxMouseFunction2->setCurrentItem(j);
            j++;
        }
    }
    else
    {
        ComboBoxMouseFunction2->setEnabled(false);
        pushButtonMouseCalibrate2->setEnabled(false);
    }*/
}

void DlgPrefMidi::slotApply()
{
//    qDebug() << "apply";

    m_pConfig->set(ConfigKey("[Midi]","File"), ConfigValue(ComboBoxMidiconf->currentText().append(".midi.xml")));
    m_pConfig->set(ConfigKey("[Midi]","Device"), ConfigValue(ComboBoxMididevice->currentText()));
    m_pConfig->set(ConfigKey("[Controls]","PowerMateFunction1"), ConfigValue(ComboBoxPowerMate1->currentText()));
    m_pConfig->set(ConfigKey("[Controls]","PowerMateFunction2"), ConfigValue(ComboBoxPowerMate2->currentText()));
  //  m_pConfig->set(ConfigKey("[Controls]","HerculesMapping"), ConfigValue(ComboBoxHercules->currentText()));

    // Since things can go wrong during this midi code (like hangs) move it
    // into another thread so we can do something sensible if it breaks

    // On Linux having the midi init in a different thread seems to fix things
    // for a number of buggy drivers, however it causes extra problems for windows
    // go figure...

    // Close MIDI
#ifndef __LINUX__
#ifndef __WIN32__
    // Deadly hack attack
    m_pMidi->devClose();
#endif
    // Change MIDI configuration
    //m_pMidiConfig->clear(); // (is currently not implemented correctly)

    // Open MIDI device
    m_pMidi->devOpen(m_pConfig->getValueString(ConfigKey("[Midi]","Device")));
#else
    MidiWorkaround mw(m_pMidi, m_pConfig, m_pMidiConfig, this);
    mw.start();
    if (mw.wait(2000)) {
        qDebug() << "Midi OK (Workaround not required)";
    } else {
        qDebug() << "ERROR: Midi driver not responding";
        // I Apologise for what happens now, I don't understand the code well enough to better (yet):
        delete m_pMidi;
        m_pMidi = new MidiObjectNull(m_pConfig->getValueString(ConfigKey("[Midi]","Device")));
    }
#endif

    // Apply the MIDI controller mappings
    setupMappings(m_pConfig->getValueString(ConfigKey("[Config]","Path")).append("midi/").append(m_pConfig->getValueString(ConfigKey("[Midi]","File"))));
    m_pMidi->setMidiConfig(m_pMidiConfig);

    // PowerMates
    if (m_pPowerMate1)
        m_pPowerMate1->selectMapping(ComboBoxPowerMate1->currentText());
    if (m_pPowerMate2)
        m_pPowerMate2->selectMapping(ComboBoxPowerMate2->currentText());

    // Hercules
/*
    if (m_pHercules)
        m_pHercules->selectMapping(ComboBoxHercules->currentText());
*/

/*    // Mice
    Mouse::destroyAll();
    if (ComboBoxMouseDevice1->currentText()!="None")
    {
        Mouse * p = 0;
#ifdef __LINUX__
        p = (Mouse *)new MouseLinux();
#endif
        if (p && p->opendev(ComboBoxMouseDevice1->currentText()))
        {
            p->selectMapping(ComboBoxMouseFunction1->currentText());
            double c = m_pConfig->getValueString(ConfigKey("[Controls]","MouseCalibration1")).toDouble();
            if (c!=0.)
                p->setCalibration(c);
        }
        else
            ComboBoxMouseDevice1->setCurrentItem(0);
    }
    if (ComboBoxMouseDevice2->currentText()!="None")
    {
        Mouse * p = 0;
#ifdef __LINUX__
        p = (Mouse *)new MouseLinux();
#endif
        if (p && p->opendev(ComboBoxMouseDevice2->currentText()))
        {
            p->selectMapping(ComboBoxMouseFunction2->currentText());
            double c = m_pConfig->getValueString(ConfigKey("[Controls]","MouseCalibration2")).toDouble();
            if (c!=0.)
                p->setCalibration(c);
        }
        else
            ComboBoxMouseDevice2->setCurrentItem(0);
    }

    // Write mouse config
    //m_pConfig->set(ConfigKey("[Controls]","MouseDevice1"), ConfigValue(ComboBoxMouseDevice1->currentText()));
    //m_pConfig->set(ConfigKey("[Controls]","MouseFunction1"), ConfigValue(ComboBoxMouseFunction1->currentText()));
    //m_pConfig->set(ConfigKey("[Controls]","MouseDevice2"), ConfigValue(ComboBoxMouseDevice2->currentText()));
    //m_pConfig->set(ConfigKey("[Controls]","MouseFunction2"), ConfigValue(ComboBoxMouseFunction2->currentText()));*/


#ifdef __C_METRICS__
        QByteArray baMIDIDevice = m_pConfig->getValueString(ConfigKey("[Midi]","Device")).toUtf8();
	    cm_writemsg_utf8(MIXXXCMETRICS_MIDI_DEVICE, baMIDIDevice.data());
#endif	
    

    slotUpdate();
}

void DlgPrefMidi::slotMouseCalibrate1()
{
    /*Mouse::destroyAll();
    if (ComboBoxMouseDevice1->currentText()!="None")
    {
#ifdef __LINUX__
        m_pMouseCalibrate = (Mouse *)new MouseLinux();
#endif
        if (m_pMouseCalibrate && m_pMouseCalibrate->opendev(ComboBoxMouseDevice1->currentText()))
            m_pMouseCalibrate->selectMapping(ComboBoxMouseFunction1->currentText());
        else
            ComboBoxMouseDevice1->setCurrentItem(0);
    }

    m_iProgress = 0;
    m_pProgressDialog->setProgress(0);

    m_pTimer->start(100);

    // Start measurement
    m_pMouseCalibrate->calibrateStart();*/
}

void DlgPrefMidi::slotMouseCalibrate2()
{
}

void DlgPrefMidi::slotMouseHelp()
{
    QMessageBox::information(0, "Mouse help", "Additional mice can be used to control playback in Mixxx. You can use<br>"
                             "the mouse in the usual way, on a table surface, and scratch or adjust<br>"
                             "phase of playback. You can also use the mouse as a sensor for a<br>"
                             "turntable. Attach the optical mouse just above the platter of the<br>"
                             "turntable so that the mouse senses the platter movement on the x <br>"
                             "axis of the mouse. Then start the turntable at normal speed (33 RPM)<br>"
                             "and press calibrate. Now you can use the turntable to control playback<br>"
                             "in Mixxx instead of using the play button. The calibration is also<br>"
                             "needed when using the mouse on a table. Move the mouse at the pace you<br>"
                             "want to map to normal playback speed, and press calibrate. Keep moving<br>"
                             "the mouse at a steady pace until the calibration is finished.", "Ok");
}

void DlgPrefMidi::slotUpdateProgressBar()
{
    m_iProgress++;
    m_pProgressDialog->setProgress(m_iProgress);

    if (m_iProgress==30)
    {
        m_pTimer->stop();
        double c = m_pMouseCalibrate->calibrateEnd();
        m_pConfig->set(ConfigKey("[Controls]","MouseCalibration1"), ConfigValue(QString("%1").arg(c)));

        slotApply();
    }
}

void DlgPrefMidi::slotCancelCalibrate()
{
    m_pTimer->stop();
    slotApply();
}

/*
   void DlgPrefMidi::setupPowerMates()
   {
   }

   void DlgPrefMidi::setupMouse()
   {
   }

   void DlgPrefMidi::setupHercules()
   {
   }
 */

MidiWorkaround::MidiWorkaround(MidiObject * pMidi, \
                               ConfigObject<ConfigValue>* pConfig, \
                               ConfigObject<ConfigValueMidi>* pMidiConfig, \
                               DlgPrefMidi * parent) {

    m_pMidi = pMidi;
    m_pConfig = pConfig;
    m_pMidiConfig = pMidiConfig;
    m_parent = parent;
}

void MidiWorkaround::run() {

    // Close MIDI
    m_pMidi->devClose();
    // Change MIDI configuration
    //m_pMidiConfig->clear(); // (is currently not implemented correctly)
    // BJW: Don't think this is necessary (or effective) here
    // m_parent->setupMappings(m_pConfig->getValueString(ConfigKey("[Config]","Path")).append("midi/").append(m_pConfig->getValueString(ConfigKey("[Midi]","File"))));
    // Open MIDI device
    m_pMidi->devOpen(m_pConfig->getValueString(ConfigKey("[Midi]","Device")));
}

void DlgPrefMidi::setupMappings(QString path) {
    qDebug() << "setupMappings(" << path << ")";
    QDomElement doc = WWidget::openXMLFile(path, "controller");
    m_pMidiConfig = new ConfigObject<ConfigValueMidi>(doc.namedItem("controls"));
    MidiLedHandler::destroyHandlers();
    MidiLedHandler::createHandlers(doc.namedItem("lights"), m_pMidi);
    m_pMidi->setMidiConfig(m_pMidiConfig);
}

