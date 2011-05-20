/// \file  
/// \date 2008-10-14
/// \author Xavier Michelon, Toradex SA 
///  
/// \brief Declaration of Oak HID base function
///
/// These function encapsulate the Linux hiddev calls


#include "OakHidBase.h"
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <string>
#include <unistd.h>
#include <sys/ioctl.h>
#include <asm/types.h>
#include <linux/hiddev.h>
#include <errno.h>


using namespace std;


namespace Toradex { namespace Oak {


//************************************************************************************************* 
/// \param[in] devicePath The path to the device (for instance "/dev/usb/hiddev0")
/// \param[in] outDeviceHandle A handle to the openened device. On failure, value is undetermined
/// \return A status code for the operation
//************************************************************************************************* 
EOakStatus openDevice(std::string const& devicePath, int& outDeviceHandle)
{
   /// \todo implement informative error reporting...
   if ((outDeviceHandle = open(devicePath.c_str(), O_RDONLY)) < 0)
      return eOakStatusErrorOpeningFile;
  
  // Initialise the internal report structures (according to the latest linux docs, this should
  // already have been done by the HID drivers in the latest kernel, so it may be redundant...)
   if (ioctl(outDeviceHandle, HIDIOCINITREPORT,0) < 0) 
   {
      close(outDeviceHandle);
      return eOakStatusInternalError;
   }
   struct hiddev_devinfo devInfo;
   ioctl(outDeviceHandle, HIDIOCGDEVINFO, &devInfo);
   if (kToradexVendorID != devInfo.vendor)
   {
      closeDevice(outDeviceHandle);
      return eOakStatusInvalidDeviceType;
   }
   return eOakStatusOK;
}


//************************************************************************************************* 
/// \param[in] deviceHandle the handle of the device to close
/// \return A status code for the operation
//************************************************************************************************* 
EOakStatus closeDevice(int deviceHandle)
{
   return (0 == close(deviceHandle)) ? eOakStatusOK : eOakStatusInternalError;
}


//************************************************************************************************* 
/// \param[in] deviceHandle The handle of the device
/// \param[out] outName The name of the device
/// \return A status code for the operation
//************************************************************************************************* 
EOakStatus getDeviceName(int deviceHandle, std::string& outName)
{
   int const kBufferSize = 256;
   char buffer[kBufferSize];
   if (ioctl(deviceHandle, HIDIOCGNAME(kBufferSize), buffer) < 0) return eOakStatusInternalError;
   outName = string(buffer);
   return eOakStatusOK;
}


//************************************************************************************************* 
/// \brief Utility function that return a string descriptor for a device
/// 
/// \param[in] deviceHandle the handle of the device
/// \param[in] index The index of the string
/// \param[out] outStr The retrieved string
/// \return A status code for the operation
//************************************************************************************************* 
EOakStatus getStringDescriptor(int deviceHandle, int index, string& outStr)
{
   hiddev_string_descriptor desc;
   desc.index = index;
   if (ioctl(deviceHandle, HIDIOCGSTRING, &desc) < 0) return eOakStatusInvalidStringDescriptorIndex;
   outStr = string(desc.value);
   return eOakStatusOK;
}


//************************************************************************************************* 
/// \param[in] device the handle of the device
/// \param[out] The serial number of the device
/// \return A status code for the operation
//************************************************************************************************* 
EOakStatus getDeviceSerialNumber(int deviceHandle, string& outDeviceSerial)
{
   return getStringDescriptor(deviceHandle, 3, outDeviceSerial);
}


//************************************************************************************************* 
/// \brief Retrieve the number of channels of the device
///
/// \param[in] deviceHandle The handle of the device
/// \param[out] outChannelCount The number of channel of the device
/// \return A status code for the operation
//************************************************************************************************* 
EOakStatus getNumberOfChannels(int deviceHandle, int& outChannelCount)
{
   hiddev_report_info rinfo;
   rinfo.report_type = HID_REPORT_TYPE_INPUT;
   rinfo.report_id = HID_REPORT_ID_FIRST;
   if (ioctl(deviceHandle, HIDIOCGREPORTINFO, &rinfo) < 0) return eOakStatusInternalError;
   outChannelCount = rinfo.num_fields;
   return eOakStatusOK;
}


//************************************************************************************************* 
/// Insertion is done using the Toradex convention: string starts at byte 5 and must not exceed
/// a length of 20 character (the function will clamp longer strings if necessary).

/// \param[in] report The report in which the string is to be placed
/// \param[in] theString the string to put in the report
/// \return A status code for the operation
//************************************************************************************************* 
EOakStatus putStringInReport(OakFeatureReport report, string const& theString)
{
   string str(theString);
   if (str.size() > 20)
      str = str.substr(0, 20);
   memcpy(report + 5, str.c_str(), str.size());
   return eOakStatusOK;
}


//************************************************************************************************* 
/// strings in feature report sent by a device start by conventions at byte 1 and their
/// length cannot exceed 20 characters
/// \param[in] report The report containing the string to extract
/// \param[out] outString The extracted string
/// \return A status code for the operation
//************************************************************************************************* 
EOakStatus getStringFromReport(OakFeatureReport report, string& outString)
{
   report[21] = 0; // buffer overflow safety
   outString = string((const char *)report + 1);
   return eOakStatusOK;
}


//************************************************************************************************* 
/// \param[in] deviceHandle the device handle
/// \param[out] outDeviceUserName the retrieved device name
/// \param[in] persistent Should the function set the persistent value of the setting 
/// (i.e. saved in Flash) or not (saved in ROM)
/// \return A status code for the operation
//************************************************************************************************* 
EOakStatus getUserDeviceName(int deviceHandle, string& outDeviceUserName, bool persistent)
{
   OakFeatureReport report = { 1, persistent ? 1 : 0, 0x15, 0, 0 };
   sendReportAndWaitForReply(deviceHandle, report);
   return getStringFromReport(report, outDeviceUserName);
}


//************************************************************************************************* 
/// \param[in] deviceHandle The handle of the device
/// \param[out] outDeviceInfo A structure containing the retrieved information
/// \return A status code for the operation
//************************************************************************************************* 
EOakStatus getDeviceInfo(int deviceHandle, DeviceInfo& outDeviceInfo)
{
   struct hiddev_devinfo devInfo;
   if (ioctl(deviceHandle, HIDIOCGDEVINFO, &devInfo) < 0) return eOakStatusInternalError;
   outDeviceInfo.vendorID = devInfo.vendor;
   outDeviceInfo.productID = devInfo.product;
   outDeviceInfo.version   = devInfo.version;   
   EOakStatus status;
   status = getDeviceName(deviceHandle, outDeviceInfo.deviceName);
   if (eOakStatusOK != status) return status;
   status = getUserDeviceName(deviceHandle, outDeviceInfo.persistentUserDeviceName, true);
   if (eOakStatusOK != status) return status;
   status = getUserDeviceName(deviceHandle, outDeviceInfo.volatileUserDeviceName, false);
   if (eOakStatusOK != status) return status;
   status = getDeviceSerialNumber(deviceHandle, outDeviceInfo.serialNumber);
   if (eOakStatusOK != status) return status;
   status = getNumberOfChannels(deviceHandle, outDeviceInfo.numberOfChannels);
   return status;
}     
                              

//************************************************************************************************* 
/// \brief Retrieve the hard coded name of a channel
///
/// \param[in] deviceHandle the device handle
/// \param[in] channelIndex The target channel
/// \param[out] outChannelName A string containing the hard coded name of the channel 
/// \return A status code for the operation
//************************************************************************************************* 
EOakStatus getChannelName(int deviceHandle, int channelIndex, string& outChannelName)
{
   return getStringDescriptor(deviceHandle, 4 + channelIndex, outChannelName);
}


//************************************************************************************************* 
/// \param[in] deviceHandle the device handle
/// \param[in] channelIndex The target channel
/// \param[out] outUserChannelName the retrieved device name
/// \param[in] persistent Should the function set the persistent value of the setting 
/// (i.e. saved in Flash) or not (saved in ROM)
/// \return A status code for the operation
//************************************************************************************************* 
EOakStatus getUserChannelName(int deviceHandle, int channelIndex, string& outUserChannelName, 
                       bool persistent)
{
   OakFeatureReport report = { 1, persistent ? 1 : 0, 0x15, (unsigned char)(channelIndex), 0 };
   sendReportAndWaitForReply(deviceHandle, report);
   return getStringFromReport(report, outUserChannelName);
}


//************************************************************************************************* 
/// \brief Retrieve the field info for a given channel
///
/// \param[in] deviceHandle The handle of the device
/// \param[in] channelIndex The index of the target channel
/// \param[out] finfo the retrieved hiddev field info structure
/// \return a status code for the operation
//************************************************************************************************* 
EOakStatus getFieldInfo(int deviceHandle, int channelIndex, hiddev_field_info& finfo)
{
   finfo.report_type = HID_REPORT_TYPE_INPUT;
   finfo.report_id = HID_REPORT_ID_FIRST;
   finfo.field_index = channelIndex;
   if (ioctl(deviceHandle, HIDIOCGFIELDINFO, &finfo) < 0)
       return eOakStatusInternalError;
   else
       return eOakStatusOK;
}


//************************************************************************************************* 
/// \brief Return the human readable exponent of a HID field
///
/// \param[in] finfo The field info
/// \return the field unit exponent
//************************************************************************************************* 
int getFieldExponent(hiddev_field_info& finfo)
{
   return (finfo.unit_exponent >= 8) ? finfo.unit_exponent - 16 : finfo.unit_exponent; 
}


//************************************************************************************************* 
/// \brief test if the given HID field is signed
///
/// \param[in] finfo the field info structure as retrieve by the hiddev API
/// \return true iff the field is signed
//************************************************************************************************* 
bool isFieldSigned(hiddev_field_info const& finfo)
{
   return (finfo.physical_minimum < 0);
}


//************************************************************************************************* 
/// \brief Compute and return the Bit size of a given HID field
///
/// \param[in] finfo the field info structure as retrieve by the hiddev API
/// \return The size in bits of the field
//**************************************he *********************************************************** 
int getFieldBitSize(hiddev_field_info const& finfo)
{
   long long physicalRange = finfo.physical_maximum - finfo.physical_minimum;
   if (physicalRange <= 0xff) return 8;
   if (physicalRange <= 0xffff) return 16;
   if (physicalRange <= 0xffffffff) return 32;
   return 64;
}


//************************************************************************************************* 
/// \retrieve the fancy name of a HID field unit
/// 
/// \param[in] deviceHandle The handle of the device
/// \param[in] finfo the field info structure as retrieve by the hiddev API
/// \param[out] outFieldUnit A string containing the name of the unit, or a null string if an 
/// error occured
/// \return A status code for the operation
//************************************************************************************************* 
EOakStatus getFieldUnit(int deviceHandle, int channelIndex, string& outFieldUnit)
{
   // Unlike the Windows version, there is no way to retrieve the 'fancy' name of the unit.
   // The hack to cope with the issue consist in extracting the unit name from the channel 
   // name where in it present between brackets
   string name;
   EOakStatus status = getChannelName(deviceHandle, channelIndex, name);
   if (eOakStatusOK != status) return status;
   int begin = name.rfind('[');
   if (begin == (signed)string::npos) return eOakStatusInternalError;
   begin++; // the unit name start just after the opening bracket
   int end = name.rfind(']');
   if (end == (signed)string::npos) return eOakStatusInternalError;
   if (end <= begin) return eOakStatusInternalError;
   outFieldUnit = name.substr(begin, end - begin);
   return eOakStatusOK;
}


//************************************************************************************************* 
/// \param[in] deviceHandle The handle of the device
/// \param[in] channelIndex The zero based index of the channel
/// \param[out] outChannelInfo The information retrieved about the channel
/// \return A status code for the operation
//************************************************************************************************* 
EOakStatus getChannelInfo(int deviceHandle, int channelIndex, ChannelInfo& outChannelInfo)
{
   EOakStatus status = getChannelName(deviceHandle, channelIndex, outChannelInfo.channelName);
   if (eOakStatusOK != status) return status;
   getUserChannelName(deviceHandle, channelIndex, outChannelInfo.persistentUserChannelName, true);
   getUserChannelName(deviceHandle, channelIndex, outChannelInfo.volatileUserChannelName, false);
   hiddev_field_info finfo;
   getFieldInfo(deviceHandle, channelIndex, finfo);
   outChannelInfo.isSigned = isFieldSigned(finfo);
   outChannelInfo.bitSize = getFieldBitSize(finfo);
   outChannelInfo.unitExponent = getFieldExponent(finfo);
   outChannelInfo.unitCode = finfo.unit;
   status = getFieldUnit(deviceHandle, channelIndex, outChannelInfo.unit);
   return status;
}


//************************************************************************************************* 
/// \brief Read an interrupt report and put the read values inside the outReadValues vector
///
/// \note This function is blocking until a interrupt report is received
/// \note This function will work optimally if the outReadValue is already of the appropriate size
/// otherwise a resize of the vector will occur. In normal case this is not a problem, as a standard
/// usage of the function will be to declare a vector and pass to every call of the function. As 
/// Toradex use fixed interrupt report size, the resize will only occur when the function is called
/// for the first time
///
/// \param[in] deviceHandle The handle to the device
/// \param[out] outReadValues A vector containing the value sent by the interrupt report
/// \return A status code for the operation
//************************************************************************************************* 
EOakStatus readInterruptReport(int deviceHandle, std::vector<int>& outReadValues)
{
   static struct hiddev_event ev[64];

   int rd = read(deviceHandle, ev, sizeof(ev));
   if (rd < (int) sizeof(ev[0])) return eOakStatusReadError;
   int const kCount = rd / sizeof(ev[0]);
   if (kCount != (signed)outReadValues.size())
      outReadValues.resize(kCount);      
   for (int i =0; i < kCount; i++)
      outReadValues[i] = ev[i].value;
   return eOakStatusOK;
}


//************************************************************************************************* 
/// \param[in] deviceHandle a handle to the device
/// \param[in] report The report to send
/// \return A status code for the operation
//************************************************************************************************* 
EOakStatus sendFeatureReport(int deviceHandle, OakFeatureReport report)
{
   struct hiddev_usage_ref_multi uref;   
   uref.uref.report_type = HID_REPORT_TYPE_FEATURE;
   uref.uref.report_id = 0;
   uref.uref.field_index = 0;
   uref.uref.usage_index = 0;
   uref.num_values = kFeatureReportSize;
   for (int i = 0; i < kFeatureReportSize; ++i)
   uref.values[i] = int(report[i]);
   if (ioctl(deviceHandle, HIDIOCSUSAGES, &uref) == -1) return eOakStatusWriteError;
   
   struct hiddev_report_info rinfo;
   rinfo.report_type = HID_REPORT_TYPE_FEATURE;
   rinfo.report_id = 0;
   rinfo.num_fields = 1;
   if (ioctl(deviceHandle, HIDIOCSREPORT, &rinfo) == -1) return eOakStatusWriteError;
   return eOakStatusOK;
}


//************************************************************************************************* 
/// \param[in] deviceHandle The device handle
/// \param[out] report The read report
/// \return A status code for the operation
//*************************************************************************************************
EOakStatus readFeatureReport(int deviceHandle, OakFeatureReport report)
{
   struct hiddev_report_info rinfo;
   rinfo.report_type = HID_REPORT_TYPE_FEATURE;
   rinfo.report_id = 0;
   rinfo.num_fields = 1;
   if (ioctl(deviceHandle, HIDIOCGREPORT, &rinfo) == -1) return eOakStatusReadError;
   struct hiddev_usage_ref_multi uref;
   uref.uref.report_type = HID_REPORT_TYPE_FEATURE;
   uref.uref.report_id = 0;
   uref.uref.field_index = 0;
   uref.uref.usage_index = 0;
   uref.num_values = kFeatureReportSize;
   if (ioctl(deviceHandle, HIDIOCGUSAGES, &uref) == -1) return eOakStatusReadError;
   for (int i = 0; i < kFeatureReportSize; ++i)
     report[i] = (unsigned char)(uref.values[i]);
   return eOakStatusOK;
}

//************************************************************************************************* 
/// This function is implemented mostly for debugging purposes
///
/// \param[in] report The feature report to display
/// \return A status code for the operation
//*************************************************************************************************
EOakStatus printFeatureReport(OakFeatureReport report)
{
   for (int i = 0; i < 32; i++)
      printf("%d ", report[i]);
   printf("\n");   
   return eOakStatusOK;
}


//************************************************************************************************* 
/// This function implement the bidirectional communicated protocol defined by Toradex. See the
/// Toradex Oak Programming guid for details. 
/// http://www.toradex.com/@api/deki/files/89/=Oak_ProgrammingGuide_V0100.pdf
///
/// \param[in] deviceHandle The device handle
/// \param[in,out] report The report to send. On function exit, this variable contains
/// the reply received from the device
/// \return A status code for the operation
//*************************************************************************************************
EOakStatus sendReportAndWaitForReply(int deviceHandle, OakFeatureReport report)
{
   static OakFeatureReport tempReport; // static to avoid realloaction at every function call...
   // read feature report until the devil is ready to receive on (i.e when byte 0 is 0xff)
   EOakStatus status;
   do 
   {
      status = readFeatureReport(deviceHandle, tempReport);
      if (eOakStatusOK != status) return status;
   } while (0xff != tempReport[0]);
   
   status = sendFeatureReport(deviceHandle, report);
   if (eOakStatusOK != status) return status;
   
   // wait until we got a valid feature report as a reply (i.e. one with first byte set to 0xff
   do
   {
      status = readFeatureReport(deviceHandle, report);
      if (eOakStatusOK != status) return status;
   } while (0xff != report[0]);
   return eOakStatusOK;
}


//************************************************************************************************* 
/// \param[in] statusCode The status code
/// \return A string containing a description of the status code
//************************************************************************************************* 
std::string getStatusString(EOakStatus statusCode)
{
   switch (statusCode)
   {
   case eOakStatusOK:                              return string("No error");
   case eOakStatusErrorOpeningFile:                return string("The device could not be opened");
   case eOakStatusInvalidDeviceType:               return string("The device is not an Oak sensor");
   case eOakStatusInternalError:                   return string("Internal error");
   case eOakStatusInvalidStringDescriptorIndex:    return string("Invalid string descriptor index");
   case eOakStatusReadError:                       return string("Read error");
   case eOakStatusWriteError:                      return string("Write error");
   default:                                        return string("Unknown error");
   }
}


} } // namespace Toradex::Oak
