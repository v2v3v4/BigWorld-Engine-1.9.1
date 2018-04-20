/******************************************************************************
BigWorld Technology
Copyright BigWorld Pty, Ltd.
All Rights Reserved. Commercial in confidence.

WARNING: This computer program is protected by copyright law and international
treaties. Unauthorized use, reproduction or distribution of this program, or
any portion of this program, may result in the imposition of civil and
criminal penalties as provided by law.
******************************************************************************/

#include "pch.hpp"
#include "gui_utilities.hpp"

float StringToFloat(CString stringValue)
{
	return (float)(atof(stringValue.GetBuffer()));
}

int StringToInt(CString stringValue)
{
	return (int)(atoi(stringValue.GetBuffer()));
}

CString FloatToString(float value)
{
	char tempString[20];
	bw_snprintf(tempString, sizeof(tempString), "%.2f", value);
	return CString(tempString);
}

CString IntToString(int value)
{
	char tempString[20];
	bw_snprintf(tempString, sizeof(tempString), "%d", value);
	return CString(tempString);
}

COLORREF RGBtoCOLORREF(const Vector4 & color)
{
	return RGB((BYTE)(color.x*255), (BYTE)(color.y*255), (BYTE)(color.z*255));
}

Vector4 COLORREFtoRGB(const COLORREF & colorREF)
{
	return 
        Vector4
        (
            GetRValue(colorREF)/255.f, 
            GetGValue(colorREF)/255.f, 
            GetBValue(colorREF)/255.f, 
            1.0f
        );
}

void 
GetFilenameAndDirectory
(
    CString     const &longFilename, 
    CString     &filename, 
    CString     &directory
)
{
	filename  = "";
	directory = "";

	if (longFilename == "")
		return;

	char directorySeperator = '/';
	int lastSeperator = longFilename.ReverseFind(directorySeperator);
	int stringLength  = longFilename.GetLength();
	
	if (lastSeperator == stringLength)
	{
		// this is a directory
		directory = longFilename;
	}
	else if (lastSeperator == -1)
	{
		// this is a filename
		filename = longFilename;
	}
	else
	{
		filename  = longFilename.Right(stringLength - lastSeperator - 1);
		directory = longFilename.Left(lastSeperator + 1);

		// make sure only one directory seperator
		directory.TrimRight("/");
		directory += "/";
	}
}

void 
PopulateComboBoxWithFilenames
(
    CComboBox               &theBox, 
    std::string             const &dir, 
    PopulateTestFunction    test
)
{
    theBox.ResetContent();
    theBox.ShowWindow(SW_HIDE);
    DataSectionPtr dataSection = BWResource::openSection(dir);    
    if (dataSection)
    {
        theBox.InitStorage(dataSection->countChildren(), 32);

        int idx = 0;
        for
        (
            DataSectionIterator it = dataSection->begin();
            it != dataSection->end();
            ++it
        )
        {
            std::string filename = (*it)->sectionName();
			std::string fullname = dir + filename;
            if (test == NULL || test( fullname ))
            {
                theBox.InsertString( idx, filename.c_str() );
                ++idx;
            }
        }
    }
    theBox.ShowWindow(SW_SHOW);
}
