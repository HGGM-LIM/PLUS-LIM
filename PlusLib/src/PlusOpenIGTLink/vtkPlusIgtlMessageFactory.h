/*=Plus=header=begin======================================================
  Program: Plus
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================Plus=header=end*/ 

#ifndef __vtkPlusIgtlMessageFactory_h
#define __vtkPlusIgtlMessageFactory_h

#include "PlusConfigure.h"
#include "vtkPlusOpenIGTLinkExport.h"

#include "vtkObject.h" 
#include "igtlMessageBase.h"
#include "PlusIgtlClientInfo.h" 

class vtkXMLDataElement; 
class TrackedFrame; 
class vtkTransformRepository; 


/*!
  \class vtkPlusIgtlMessageFactory 
  \brief Factory class of supported OpenIGTLink message types

  This class is a factory class of supported OpenIGTLink message types to localize the message creation code.

  \ingroup PlusLibOpenIGTLink
*/ 
class vtkPlusOpenIGTLinkExport vtkPlusIgtlMessageFactory: public vtkObject
{
public:
  static vtkPlusIgtlMessageFactory *New();
  vtkTypeMacro(vtkPlusIgtlMessageFactory,vtkObject);
  virtual void PrintSelf(ostream& os, vtkIndent indent);

  /*! Function pointer for storing New() static methods of igtl::MessageBase classes */ 
  typedef igtl::MessageBase::Pointer (*PointerToMessageBaseNew)(); 

  /*! 
  Add message type name and pointer to IGTL message new function 
  Usage: AddMessageType( "IMAGE", (PointerToMessageBaseNew)&igtl::ImageMessage::New );  
  \param messageTypeName The name of the message type
  \param messageTypeNewPointer Function pointer to the message type new function (e.g. (PointerToMessageBaseNew)&igtl::ImageMessage::New )
  */ 
  virtual void AddMessageType(std::string messageTypeName, vtkPlusIgtlMessageFactory::PointerToMessageBaseNew messageTypeNewPointer); 

  /*! 
  Get pointer to message type new function, or NULL if the message type not registered 
  Usage: igtl::MessageBase::Pointer message = GetMessageTypeNewPointer("IMAGE")(); 
  */ 
  virtual vtkPlusIgtlMessageFactory::PointerToMessageBaseNew GetMessageTypeNewPointer(std::string messageTypeName); 

  /*! Print all supported OpenIGTLink message types */
  virtual void PrintAvailableMessageTypes(ostream& os, vtkIndent indent);

  /*! Create a new igtl::MessageBase instance from message type, delete previous igtl::MessageBase if's not NULL */ 
  PlusStatus CreateInstance(const char* aIgtlMessageType, igtl::MessageBase::Pointer& aMessageBase);

  /*! 
  Generate and pack IGTL messages from tracked frame
  \param packValidTransformsOnly Control whether or not to pack transform messages if they contain invalid transforms
  \param clientInfo Specifies list of message types and names to generate for a client.
  \param igtMessages Output list for the generated IGTL messages
  \param trackedFrame Input tracked frame data used for IGTL message generation 
  \param transformRepository Transform repository used for computing the selected transforms 
  */ 
  PlusStatus PackMessages(const PlusIgtlClientInfo& clientInfo, std::vector<igtl::MessageBase::Pointer>& igtMessages, TrackedFrame& trackedFrame, 
    bool packValidTransformsOnly, vtkTransformRepository* transformRepository=NULL); 

protected:
  vtkPlusIgtlMessageFactory();
  virtual ~vtkPlusIgtlMessageFactory();
  
  /*! Map igt message types and the New() static methods of igtl::MessageBase classes */ 
  std::map<std::string,PointerToMessageBaseNew> IgtlMessageTypes; 

private:
  vtkPlusIgtlMessageFactory(const vtkPlusIgtlMessageFactory&);
  void operator=(const vtkPlusIgtlMessageFactory&);

}; 

#endif 