/*=Plus=header=begin======================================================
  Program: Plus
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================Plus=header=end*/

#ifndef __vtkNrrdSequenceIO_h
#define __vtkNrrdSequenceIO_h

#include "vtkPlusCommonExport.h"

#ifdef _MSC_VER
#pragma warning ( disable : 4786 )
#endif 

#include "vtkSequenceIOBase.h"
#include "vtk_zlib.h"

class vtkTrackedFrameList;

/*!
  \class vtkNrrdSequenceIO
  \brief Read and write Nrrd file with a sequence of frames, with additional information for each frame. File definition can be found at http://teem.sourceforge.net/nrrd/format.html
  \ingroup PlusLibCommon
*/
class vtkPlusCommonExport vtkNrrdSequenceIO : public vtkSequenceIOBase
{
  enum NrrdEncoding
  {
    NRRD_ENCODING_RAW = 0,
    NRRD_ENCODING_TXT,
    NRRD_ENCODING_TEXT = NRRD_ENCODING_TXT,
    NRRD_ENCODING_ASCII = NRRD_ENCODING_TXT,
    NRRD_ENCODING_HEX,
    NRRD_ENCODING_GZ, 
    NRRD_ENCODING_GZIP = NRRD_ENCODING_GZ,
    NRRD_ENCODING_BZ2,
    NRRD_ENCODING_BZIP2 = NRRD_ENCODING_BZ2
  };

public:
  static vtkNrrdSequenceIO *New();
  vtkTypeMacro(vtkNrrdSequenceIO, vtkSequenceIOBase);
  virtual void PrintSelf(ostream& os, vtkIndent indent);

  /*! Update the number of frames in the header 
      This is used primarily by vtkVirtualDiscCapture to update the final tally of frames, as it continually appends new frames to the file
      /param numberOfFrames the new number of frames to write
      /param addPadding this should only be true if this is the first time this function is called, which typically happens in OpenImageHeader
  */
  virtual PlusStatus OverwriteNumberOfFramesInHeader(int numberOfFrames, bool addPadding=false);

  /*! 
    Append the frames in tracked frame list to the header, if the onlyTrackerData flag is true it will not save
    in the header the image data related fields. 
  */
  virtual PlusStatus AppendImagesToHeader();

  /*! Finalize the header */
  virtual PlusStatus FinalizeHeader();

  /*! Close the sequence */
  virtual PlusStatus Close();

  /*! Check if this class can read the specified file */
  static bool CanReadFile(const std::string& filename);

  /*! Check if this class can write the specified file */
  static bool CanWriteFile(const std::string& filename);

  /*! Update a field in the image header with its current value */
  virtual PlusStatus UpdateFieldInImageHeader(const char* fieldName);

  /*! Return the string that represents the dimensional sizes */
  virtual const char* GetDimensionSizeString();

  /*! Return the string that represents the dimensional sizes */
  virtual const char* GetDimensionKindsString();

  /*!
    Set input/output file name. The file contains only the image header in case of
    nhdr images and the full image (including pixel data) in case of nrrd images.
  */
  virtual PlusStatus SetFileName(const std::string& aFilename);

protected:
  vtkNrrdSequenceIO();
  virtual ~vtkNrrdSequenceIO();

  /*! Read all the fields in the image file header */
  virtual PlusStatus ReadImageHeader();

  /*! Read pixel data from the image */
  virtual PlusStatus ReadImagePixels();

  /*! Prepare the image file for writing */
  virtual PlusStatus PrepareImageFile();

  /*! Write all the fields to the image file header */
  virtual PlusStatus OpenImageHeader();

  /*! 
    Writes the compressed pixel data directly into file. 
    The compression is performed in chunks, so no excessive memory is used for the compression.
    \param compressedDataSize returns the size of the total compressed data that is written to the file.
  */
  virtual PlusStatus WriteCompressedImagePixelsToFile(int &compressedDataSize);

  /*! Conversion between ITK and METAIO pixel types */
  PlusStatus ConvertNrrdTypeToVtkPixelType(const std::string &elementTypeStr, PlusCommon::VTKScalarPixelType &vtkPixelType);
  /*! Conversion between ITK and METAIO pixel types */
  PlusStatus ConvertVtkPixelTypeToNrrdType(PlusCommon::VTKScalarPixelType vtkPixelType, std::string &elementTypeStr);

  /*! Return the size of a file */
  static FilePositionOffsetType GetFileSize(const std::string& filename);

  /*! Convert a string to an encoding */
  static NrrdEncoding StringToNrrdEncoding(const std::string& encoding);

  /*! Convert an encoding to a string*/
  static std::string NrrdEncodingToString(NrrdEncoding encoding);

private:    
  /*! Nrrd encoding type */
  NrrdEncoding Encoding;

  /*! file handle for the compression stream */
  gzFile CompressionStream;
  
protected:
  vtkNrrdSequenceIO(const vtkNrrdSequenceIO&); //purposely not implemented
  void operator=(const vtkNrrdSequenceIO&); //purposely not implemented
};

#endif // __vtkNrrdSequenceIO_h 
