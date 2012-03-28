
///////////////////////////////////////////////////////////////////////////////////////
//
//
// TreeList Control For Win32
// Create by Eitan Michaelson 4/2011 , Noyasoft@gmail.com
//
//
///////////////////////////////////////////////////////////////////////////////////////

#ifndef _TREELIST_CONTROL_H
#define _TREELIST_CONTROL_H

#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0500

#include <stdio.h>
#include <windows.h>
#include <commctrl.h>
#include <windowsx.h>

///////////////////////////////////////////////////////////////////////////////////////
//
// Section  : Defines
//
///////////////////////////////////////////////////////////////////////////////////////

#define TREELIST_APIVERSION             "1.71"
#define TREELIST_MAX_STRING             32  // Max size of long (32 bits) as a string = max of 10 characters
#define TREELIST_LAST_COLUMN            0
#define TREELIST_MAX_COLUMNS            17
#define TREELIST_INVLID_HANDLE          FALSE
#define TREELIST_MAX_INSTANCES          16 // Per window


// Control creation flags
#define TREELIST_DRAW_EDGE              0x00000010 // Draw a rect around the control
#define TREELIST_ANCHOR_RIGHT           0x00000020 // Anchor - auto resize 
#define TREELIST_ANCHOR_BOTTOM          0x00000040 // Anchor - auto resize 
#define TREELIST_NORMAL_EDITBOX         0x00000080 // Use Normal (not colored) edit boxes, this could be expanded later to more

// Handlers 
typedef void *NODE_HANDLE;              // ListTree handle (the internal session pointer)
typedef void *TREELIST_HANDLE;          // a handle to a single nodes

// Caller edit validate call back
typedef BOOL _stdcall TREELIST_CB(const TREELIST_HANDLE, const void *pAnyPtr,const char *NewData,char *Override); 

///////////////////////////////////////////////////////////////////////////////////////
//
// Section  : Error types
//
///////////////////////////////////////////////////////////////////////////////////////

typedef enum
{
    
    e_OK,
        e_ERROR_CONTROL_ACTIVE,
        e_ERROR_MEMORY_ALLOCATION,
        e_ERROR_CANT_CREATE_FONT,
        e_ERROR_CANT_GET_FONT_INFO,
        e_ERROR_CANT_CREATE_TREE_VIEW,
        e_ERROR_CANT_CREATE_HEADER,
        e_ERROR_NO_SESSION,
        e_ERROR_COULD_NOT_ADD_COLUMN,
        e_ERRI_PARAMETER,
        
}TreeListError;

///////////////////////////////////////////////////////////////////////////////////////
//
// Section  : NodeData struct
//
///////////////////////////////////////////////////////////////////////////////////////

struct tag_TreeListNodeData
{
    
    char                        Data    [TREELIST_MAX_STRING +1];
    BOOL                        Editable;
    BOOL                        Numeric;
    void                        *pExternalPtr;
    BOOL                        Colored;
    COLORREF                    BackgroundColor;
    COLORREF                    TextColor;
    COLORREF                    AltertedTextColor;
    BOOL                        Altered;            // Private struct members -->
    long                        CRC;                // Last element in struct!
    
    
};
typedef struct tag_TreeListNodeData TreeListNodeData;


///////////////////////////////////////////////////////////////////////////////////////
//
// Section  : API
//
///////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////
//
// Function:    TreeListCreate
// Description: Init the TreeList API, Allocates memory and create the internal data types
// Parameters:  HINSTANCE           Applicayion instance
//              HWND                Parent window handler
//              RECT*               a rect for a fixed size control, NULL will cause the control to be 100% of
//                                  It's parent size
//              DWORD               Flags
//              TREELIST_CB*        a pointer to a call back to validate the user edit requests (can be NULL)
// Return:      TREELIST_HANDLE     a valid handle to the listtree control 
//    
// Note:        This call will internaly subclass the parent window
//
////////////////////////////////////////////////////////////////////////////////////

TREELIST_HANDLE TreeListCreate        (HINSTANCE Instance, HWND Hwnd,RECT *pRect,DWORD dwFlags, TREELIST_CB *pFunc);


////////////////////////////////////////////////////////////////////////////////////
//
// Function:    TreeListDestroy
// Description: Delete all elements and free memory
// Parameters:  TREELIST_HANDLE a valid handle to a listtree control
// Note:        void
//          
//
////////////////////////////////////////////////////////////////////////////////////

int             TreeListDestroy         (TREELIST_HANDLE ListTreeHandle);

////////////////////////////////////////////////////////////////////////////////////
//
// Function:    TreeListAddColumn
// Description: Add a column to the TreeList control
// Parameters:  TREELIST_HANDLE a valid handle to a listtree control
//              char*           Null terminated string of the column name (up to TREELIST_MAX_STRING)
//              int             Width in pixels, last call to this function should set it to TREELIST_LAST_COLUMN
// Notes:       Max xolumns count is defined as TREELIST_MAX_COLUMNS
// Return:      TreeListError   see enum     
//
////////////////////////////////////////////////////////////////////////////////////

TreeListError   TreeListAddColumn       (TREELIST_HANDLE ListTreeHandle,char *szColumnName,int Width);


////////////////////////////////////////////////////////////////////////////////////
//
// Function:    TreeListAddNode
// Description: Add a new node
// Parameters:  TREELIST_HANDLE a valid handle to a listtree control
//              NODE_HANDLE         a Handle to the parent node, NULL when this is the first root node
//              TreeListNodeData*   an array of columns data attached to this node
//              int                 Count of elements in  TreeListNodeData*      
// Return:      NODE_HANDLE         a valid node handle , NULL on error     
//
////////////////////////////////////////////////////////////////////////////////////

NODE_HANDLE     TreeListAddNode         (TREELIST_HANDLE ListTreeHandle,NODE_HANDLE ParentHandle,TreeListNodeData *RowOfColumns,int ColumnsCount);



#endif // _TREELIST_CONTROL_H