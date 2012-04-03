///////////////////////////////////////////////////////////////////////////////////////
//
//
// TreeList Control For Win32
// Create by Eitan Michaelson 4/2011 , Noyasoft@gmail.com
//
// Revision History
//
// Version 1.71
// 4/26/2011: Fixed width of edit box when not the last column
//
///////////////////////////////////////////////////////////////////////////////////////


#pragma warning(disable : 4996) // CRT Secure - off

#include "TreeList.h"
#include "utils.hpp"
using namespace std;

template<class T>
void reset(T *&ref, T *new_value) {
  delete ref;
  ref = new_value;
}

///////////////////////////////////////////////////////////////////////////////////////
//
//
// MACRO\defines and Helpers
//
//
///////////////////////////////////////////////////////////////////////////////////////

#define TREELIST_FONT_EXTRA_HEIGHT              7
#define TREELIST_FONT_TEXT_CELL_OFFSET          9
#define TREELIST_PRNTCTLSIZE(TreeDim)           TreeDim.X,TreeDim.Y,TreeDim.Width,TreeDim.Hight
#define TREELIST_MIN(a, b)  (((a) < (b)) ? (a) : (b))
#define TREELIST_MAX(a, b)  (((a) > (b)) ? (a) : (b))

#define TREELIST_WM_EDIT_NODE                   (WM_USER + 100)
#define TREELIST_WM_EDIT_ENTER                  (WM_USER + 101)
#define TREELIST_ELEMENTS_PER_INSTANCE          4
#define TREELIST_PROP_VAL                       "TREELIST_PTR" 

//#define TREELIST_DOUBLE_BUFFEING

typedef HANDLE (CALLBACK* LPREMOVEPROP)(HWND,LPCTSTR);  // VC2010 issue


///////////////////////////////////////////////////////////////////////////////////////
//
//
// Internal tree data type
//
//
///////////////////////////////////////////////////////////////////////////////////////

// a single node (linked list) member

struct TreeListNode
{
    int                 NodeDataCount;       // Count of items in pNodeData
    HTREEITEM           TreeItemHandle;
    TreeListNode        *pParennt;
    TreeListNode        *pSibling;
    TreeListNode        *pBrother;
    TreeListNodeData    **pNodeData;         // Array of NodeData for each column
    
};

///////////////////////////////////////////////////////////////
// a column (header) struct

struct TreeListColumnInfo
{
    
    char                ColumnName[TREELIST_MAX_STRING+1];
    int                 Width;
    
};


///////////////////////////////////////////////////////////////
// It's more convenient then RECT

struct TreeListDimensions
{
    
    int                         X;
    int                         Y;
    int                         Width;
    int                         Hight;
};


///////////////////////////////////////////////////////////////
// The is the session, the internal data for a control instance

struct tag_TreeListSession
{
    
    
    HINSTANCE                   InstanceParent;
    HWND                        HwndParent;
    HWND                        HwndTreeView;
    HWND                        HwndHeader;
    HWND                        HwndEditBox;
    HFONT                       FontHandleTreeList;
    HFONT                       FontHandleHeader;
    HFONT                       FontHandleEdit;
    LOGFONT                     FontInfoTreeList;
    LOGFONT                     FontInfoHeader;
    LOGFONT                     FontInfoEdit;
    PAINTSTRUCT                 PaintStruct;
    WNDPROC                     ProcEdit;
    WNDPROC                     ProcTreeList;
    WNDPROC                     ProcParent;
    HDITEM                      HeaderItem;
    HTREEITEM                   EditedTreeItem;
    RECT                        RectParent;
    RECT                        RectTree;
    RECT                        RectHeader;
    RECT                        RectRequested;
    RECT                        RectBorder;
    RECT                        RectClientOnParent;
    HDC                         DCListView;
    HDC                         DCHeader;
    TVINSERTSTRUCT              TreeStruct;
    TreeListDimensions          SizeTree;
    TreeListDimensions          SizeHeader;
    TreeListDimensions          SizeParent;
    TreeListDimensions          SizeRequested;
    TreeListDimensions          SizeEdit;
    BOOL                        ColumnsLocked;
    BOOL                        ColumnDoAutoAdjust;
    BOOL                        WaitingForCaller;
    BOOL                        UseFullSize;
    BOOL                        UseAnchors;
    BOOL                        GotAnchors;
    BOOL                        ParentResizing;
    BOOL                        ItemWasSelected;
    DWORD                       EditBoxStyleNormal;
    DWORD                       CreateFlags;
    POINT                       PointAnchors;
    int                         ColumnsCount;
    int                         ColumnsTotalWidth;
    int                         ColumnsFirstWidth;
    int                         EditedColumn;
    int                         AllocatedTreeBytes;
    char                        EditBoxBuffer[TREELIST_MAX_STRING+1];
    char                        EditBoxOverrideBuffer[TREELIST_MAX_STRING+1];
    TreeListColumnInfo          **pColumnsInfo;
    TreeListNode                *pRootNode;
    TREELIST_CB                 *pCBValidateEdit;
};


typedef struct tag_TreeListSession TreeListSession;

///////////////////////////////////////////////////////////////

// Dictionary pointer that will hold the ref count and HWND for each instance of the control
// a pointer to the dictionary will be attached to the parent window of the control.
// This array will be updated with each instance and destroyed when the last control will be terminated.

struct tag_TreeListDict 
{
    int                         ReferenceCount;
    HWND                        HwndParent      [TREELIST_MAX_INSTANCES];
    HWND                        HwndInstances   [TREELIST_MAX_INSTANCES][TREELIST_ELEMENTS_PER_INSTANCE];
    TreeListSession             *pSessionPtr    [TREELIST_MAX_INSTANCES];
    
};
typedef tag_TreeListDict TreeListDict;


/////////////////////////////////////////////////////////////////
// Visual Studeo 2010 Can't find RemoveProc so..
static HINSTANCE hDllHandle;                    // Handle to DLL
#if _MSC_VER > 1200
static LPREMOVEPROP pRemoveProp     = 0;        // Function pointer
#else
static LPREMOVEPROP pRemoveProp = RemoveProp;
#endif


///////////////////////////////////////////////////////////////////////////////////////
//
//
// CRC32 Static tables
//
//
///////////////////////////////////////////////////////////////////////////////////////

static const unsigned long TreeListCRC32Table[] =
{
    0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,0xE963A535,
        0x9E6495A3,0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,0x09B64C2B,0x7EB17CBD,
        0xE7B82D07,0x90BF1D91,0x1DB71064,0x6AB020F2,0xF3B97148,0x84BE41DE,0x1ADAD47D,
        0x6DDDE4EB,0xF4D4B551,0x83D385C7,0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,
        0x14015C4F,0x63066CD9,0xFA0F3D63,0x8D080DF5,0x3B6E20C8,0x4C69105E,0xD56041E4,
        0xA2677172,0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,0x35B5A8FA,0x42B2986C,
        0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,0x26D930AC,
        0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F4B5,0x56B3C423,0xCFBA9599,0xB8BDA50F,
        0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,0x2F6F7C87,0x58684C11,0xC1611DAB,
        0xB6662D3D,0x76DC4190,0x01DB7106,0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,
        0x9FBFE4A5,0xE8B8D433,0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0DBB,
        0x086D3D2D,0x91646C97,0xE6635C01,0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,
        0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,0x65B0D9C6,0x12B7E950,0x8BBEB8EA,
        0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,0x4DB26158,0x3AB551CE,
        0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,0xA4D1C46D,0xD3D6F4FB,0x4369E96A,
        0x346ED9FC,0xAD678846,0xDA60B8D0,0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,
        0x5005713C,0x270241AA,0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,
        0xCE61E49F,0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,
        0xB7BD5C3B,0xC0BA6CAD,0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,0xEAD54739,
        0x9DD277AF,0x04DB2615,0x73DC1683,0xE3630B12,0x94643B84,0x0D6D6A3E,0x7A6A5AA8,
        0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,0xF00F9344,0x8708A3D2,0x1E01F268,
        0x6906C2FE,0xF762575D,0x806567CB,0x196C3671,0x6E6B06E7,0xFED41B76,0x89D32BE0,
        0x10DA7A5A,0x67DD4ACC,0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,0xD6D6A3E8,
        0xA1D1937E,0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,
        0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA867DF55,0x316E8EEF,
        0x4669BE79,0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,0xCC0C7795,0xBB0B4703,
        0x220216B9,0x5505262F,0xC5BA3BBE,0xB2BD0B28,0x2BB45A92,0x5CB36A04,0xC2D7FFA7,
        0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,
        0x9C0906A9,0xEB0E363F,0x72076785,0x05005713,0x95BF4A82,0xE2B87A14,0x7BB12BAE,
        0x0CB61B38,0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,0x86D3D2D4,0xF1D4E242,
        0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,0x88085AE6,
        0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,0xF862AE69,0x616BFFD3,0x166CCF45,
        0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,0xA7672661,0xD06016F7,0x4969474D,
        0x3E6E77DB,0xAED16A4A,0xD9D65ADC,0x40DF0B66,0x37D83BF0,0xA9BCAE53,0xDEBB9EC5,
        0x47B2CF7F,0x30B5FFE9,0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,0xBAD03605,
        0xCDD70693,0x54DE5729,0x23D967BF,0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,
        0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D
};

///////////////////////////////////////////////////////////////////////////////////////
//
// Function : TreeList_Internal_CRCCreate
// Notes    : Return a 32-bit CRC of the contents of the buffer
//
///////////////////////////////////////////////////////////////////////////////////////

static unsigned long TreeList_Internal_CRCCreate(const void *buf,
                                                 unsigned long bufLen)
{
    unsigned long   crc32;
    unsigned long   i;
    unsigned char   *byteBuf;
    
    
    // Accumulate crc32 for a buffer
    crc32 = 0 ^ 0xFFFFFFFF;
    byteBuf = (unsigned char*) buf;
    
    for (i=0; i < bufLen; i++)
    {
        crc32 = (crc32 >> 8) ^ TreeListCRC32Table[ (crc32 ^ byteBuf[i]) & 0xFF ];
    }
    
    return( crc32 ^ 0xFFFFFFFF );
    
}

///////////////////////////////////////////////////////////////////////////////////////
//
// Function : TreeList_Internal_CRCCheck
// Notes    : Validates a crc32
// Returns  : 0 - Error
//            1 - Successes
//
///////////////////////////////////////////////////////////////////////////////////////

static int TreeList_Internal_CRCCheck(const void *buf,
                                      unsigned long bufLen,
                                      unsigned long crc32)
{
    
    
    if(TreeList_Internal_CRCCreate(buf,bufLen) == crc32)
    {
        return 1;
    }
    
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////
//
// Function : TreeList_Internal_GetSessionFromDict
// Notes    : When hWndAny is NULL it will return a pointer to the dictionary
//          : When hWndParent is NULL it will be searched according to hWndAny
// Returns  : All the instances are being stored as an array of values under the parent hwnd
//
//
///////////////////////////////////////////////////////////////////////////////////////

static void *TreeList_Internal_DictGetPtr(HWND hWndParent,HWND hWndAny)
{
    
    
    int iCount      = 0;
    int iElement    = 0;
    HWND hParent    = hWndParent;
    
    TreeListDict *pTreeListDict = 0;
    TreeListSession *pSession = 0;
    
    if(!hWndParent)
        hParent = GetParent(hWndAny);
    
    pTreeListDict = (TreeListDict*)GetProp(hParent,TREELIST_PROP_VAL); // Extract the dict pointer
    if(!pTreeListDict)
        return 0; // No pointr attached to the window handler or no instances
    
    if(!hWndAny)
        return (void*)pTreeListDict;
    
    if(pTreeListDict->ReferenceCount == 0)
        return 0;
    
    for(iCount = 0;iCount<TREELIST_MAX_INSTANCES;iCount++)
    {
        
        for(iElement = 0;iElement < TREELIST_ELEMENTS_PER_INSTANCE;iElement++)
        {
            if(pTreeListDict->HwndInstances[iCount][iElement]== hWndAny)
            {
                if(pTreeListDict->pSessionPtr[iCount])
                    return (void*)pTreeListDict->pSessionPtr[iCount];
                
            }
        }
        
    }
    
    return 0;
}


///////////////////////////////////////////////////////////////////////////////////////
//
// Function : TreeList_Internal_DictUpdate
// Notes    : When hWndParent is NULL it will be searched according to hWndAny
// Notes  :   Update the dictionary pointer to hold or remove a new session
//
//
///////////////////////////////////////////////////////////////////////////////////////

static BOOL TreeList_Internal_DictUpdate(BOOL Clear,TreeListSession *pSession,HWND hWndParent,HWND hWndAny)
{
    
    
    int iCount      = 0;
    int iElement    = 0;
    HWND hParent    = hWndParent;
    int iNullsCount = 0;
    TreeListDict *pTreeListDict = 0;
    
    
    if(!hWndAny || !pSession)
        return FALSE;
    
    if(!hWndParent)
        hParent = GetParent(hWndAny); // Search for the parent
    
    
    pTreeListDict = (TreeListDict*)GetProp(hParent,TREELIST_PROP_VAL); // Extract the dict pointer from the parent
    if(Clear == FALSE)
    {
        if(!pTreeListDict)
        {
            // Allocate and attach
            pTreeListDict = (TreeListDict*)malloc(sizeof(TreeListDict));
            memset(pTreeListDict,0,sizeof(TreeListDict));
            if(SetProp(hParent,TREELIST_PROP_VAL,pTreeListDict) == FALSE)
                return FALSE;
            
            pTreeListDict->pSessionPtr[0]       = pSession;
            pTreeListDict->HwndInstances[0][0]  = hWndAny;
            pTreeListDict->HwndParent[0]        = hParent;
            pTreeListDict->ReferenceCount++;
            
            return TRUE;
        }
        
        for(iCount = 0;iCount<TREELIST_MAX_INSTANCES;iCount++)
        {
            
            // Look for the session pointer
            if(pTreeListDict->pSessionPtr[iCount] == pSession)
            {
                // found it, look if it holds our hwnd
                for(iElement = 0;iElement < TREELIST_ELEMENTS_PER_INSTANCE;iElement++)
                {
                    if(pTreeListDict->HwndInstances[iCount][iElement]== hWndAny)
                        return FALSE; // All ready there
                }
                
                // found it, update the array
                for(iElement = 0;iElement < TREELIST_ELEMENTS_PER_INSTANCE;iElement++)
                {
                    if(pTreeListDict->HwndInstances[iCount][iElement]== 0)
                    {
                        pTreeListDict->HwndInstances[iCount][iElement]= hWndAny;
                        return TRUE; // Updated
                    }
                }
                // If we are here then we could not update, hWnd array was full??
                return FALSE;
            }
        }
        
        
        // The session was not found, we have to add a new one
        for(iCount = 0;iCount<TREELIST_MAX_INSTANCES;iCount++)
        {
            
            // Look for an empty place
            if(pTreeListDict->pSessionPtr[iCount] == 0)
            {
                pTreeListDict->pSessionPtr[iCount] = pSession;
                pTreeListDict->HwndInstances[iCount][0] = hWndAny;
                pTreeListDict->HwndParent[iCount]       = hParent;
                pTreeListDict->ReferenceCount++;
                return TRUE;
            }
            
        }
        
        
        return FALSE; // could not find an empty place
    }
    else // Clear the param
    {
        
        for(iCount = 0;iCount<TREELIST_MAX_INSTANCES;iCount++)
        {
            
            // Look for the session pointer
            if(pTreeListDict->pSessionPtr[iCount] == pSession)
            {
                // found it, look if it holds our hwnd
                for(iElement = 0;iElement < TREELIST_ELEMENTS_PER_INSTANCE;iElement++)
                {
                    
                    if(pTreeListDict->HwndInstances[iCount][iElement]== hWndAny)
                    {
                        pTreeListDict->HwndInstances[iCount][iElement] = 0;
                        break;
                    }
                }
                // Reduce the ref count when all elements are deleted
                for(iElement = 0;iElement < TREELIST_ELEMENTS_PER_INSTANCE;iElement++)
                {
                    
                    if(pTreeListDict->HwndInstances[iCount][iElement]== 0)
                        iNullsCount++;
                }
                
                if(iNullsCount >= TREELIST_ELEMENTS_PER_INSTANCE)
                {
                    
                    // Clean it, reduce refcount
                    pTreeListDict->pSessionPtr[iCount] = 0;
                    pTreeListDict->HwndParent[0]       = 0;
                    pTreeListDict->ReferenceCount--;
                    if(pTreeListDict->ReferenceCount == 0)
                    {
                        free(pTreeListDict);
                        pRemoveProp(hParent,TREELIST_PROP_VAL);
                        if(hDllHandle)
                            FreeLibrary(hDllHandle);
                    }
                }
                
                return TRUE;
            }
        }
        
        return FALSE;
    }
}

///////////////////////////////////////////////////////////////////////////////////////
//
// Function : TreeList_Internal_DestroyEditBox
// Notes    : Kill and free resources related to an edit box
// Returns  :
//
//
///////////////////////////////////////////////////////////////////////////////////////

static void TreeList_Internal_DestroyEditBox(TreeListSession *pSession)
{
    
    if(!pSession)
        return; // No session
    
    // Close edit box, when moving and resizing items
    if(pSession->HwndEditBox)
    {
        
        TreeList_Internal_DictUpdate(TRUE,pSession,NULL,pSession->HwndEditBox);
        (WNDPROC)SetWindowLong(pSession->HwndEditBox, GWL_WNDPROC, (LONG)pSession->ProcEdit); // Restore the original wnd proc to the parent
        DestroyWindow(pSession->HwndEditBox);
        pSession->HwndEditBox       = 0;
        pSession->EditedTreeItem    = 0;
        pSession->EditedColumn      = 0;
        
    }
}

///////////////////////////////////////////////////////////////////////////////////////
//
// Function : TreeList_Internal_NodeGetLastSibling
// Notes    : Gets the last sibling of a given node
// Returns  :
//
//
///////////////////////////////////////////////////////////////////////////////////////

static TreeListNode* TreeList_Internal_NodeCreateNew(TreeListSession *pSession)
{
  TreeListNode    *pTmpNode = 0;
  int             iCol = 0;

  if(!pSession)
    return 0; // No session


  pTmpNode = (TreeListNode*)malloc(sizeof(TreeListNode));
  if(pTmpNode)
  {
    memset(pTmpNode,0,sizeof(TreeListNode));
    pSession->AllocatedTreeBytes += sizeof(TreeListNode);

    pTmpNode->pNodeData = (TreeListNodeData**)malloc(sizeof(TreeListNodeData*) * (pSession->ColumnsCount +1));
    if(!pTmpNode->pNodeData)
    {
      SAFE_FREE(pTmpNode);
      return pTmpNode;
    }

    for(iCol = 0;iCol< pSession->ColumnsCount;iCol++)
      pTmpNode->pNodeData[iCol] = 0;

    pSession->AllocatedTreeBytes += (sizeof(TreeListNodeData*) * pSession->ColumnsCount);

  }
  return pTmpNode;
}


///////////////////////////////////////////////////////////////////////////////////////
//
// Function : TreeList_Internal_NodeGetLastBrother
// Notes    : Gets the last brother of a given node
// Returns  :
//
//
///////////////////////////////////////////////////////////////////////////////////////

static TreeListNode* TreeList_Internal_NodeGetLastBrother(TreeListNode *pNode)
{
    
    TreeListNode *pTmpNode = pNode;
    
    if(!pNode)
        return 0; // Invalid node as passed in
    
    if(pTmpNode->pBrother)
    {
        while(pTmpNode->pBrother)
            pTmpNode = pTmpNode->pBrother;
        
        return pTmpNode;
    }
    
    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////
//
// Function : TreeList_Internal_NodeColonize
// Notes    : Updates a given node with the requested data
// Returns  :
//
//
///////////////////////////////////////////////////////////////////////////////////////

static TreeListNode* TreeList_Internal_NodeColonize(TreeListSession *pSession, TreeListNode *pNode, TreeListNodeData *pNodeData)
{
    
    if(!pSession)
        return 0; // No session
    
    if(!pNode)
        return 0; // No Node
    
    if(!pNode->pNodeData)
        return 0; // Null pointer
    
    // Allocate the new element
    pNode->pNodeData[pNode->NodeDataCount] = new TreeListNodeData(); //(TreeListNodeData*)malloc(sizeof(TreeListNodeData)); // Allocate memory
    if(!pNode->pNodeData[pNode->NodeDataCount])
        return 0; // No memory
    
    pSession->AllocatedTreeBytes += sizeof(TreeListNodeData);
    
    //memcpy(pNode->pNodeData[pNode->NodeDataCount],pNodeData,sizeof(TreeListNodeData)); // copy to the internal container
    *pNode->pNodeData[pNode->NodeDataCount] = *pNodeData;
    pNode->pNodeData[pNode->NodeDataCount]->CRC = TreeList_Internal_CRCCreate(pNode->pNodeData[pNode->NodeDataCount],
        (sizeof(TreeListNodeData) - sizeof(pNode->pNodeData[pNode->NodeDataCount]->CRC))); // Set the crc sig
    pNode->NodeDataCount++;
    
    return pNode;
}

///////////////////////////////////////////////////////////////////////////////////////
//
// Function : TreeList_Internal_AddNode
// Notes    : Add a nodeto the tree
// Returns  :
//
//
///////////////////////////////////////////////////////////////////////////////////////

static TreeListNode* TreeList_Internal_AddNode(TreeListSession *pSession,TreeListNode *pParent)
{
    
    TreeListNode *pNewNode      = 0;
    TreeListNode *pLastBrother  = 0;
    int           Nodes         = 0;
    
    
    if(!pSession)
        return 0; // No session
    
    // Special Case: Create the root node
    if(!pParent && !pSession->pRootNode)
    {
        pSession->pRootNode = TreeList_Internal_NodeCreateNew(pSession);
        return pSession->pRootNode;
    }
    
    // Special Case: Error, no root, no parent
    if(!pParent && !pSession->pRootNode)
        return 0;
    
    
    // Special Case: a root node brother
    if(!pParent)
    {
        pNewNode = TreeList_Internal_NodeCreateNew(pSession);
        if(pNewNode)
        {
            // Be the last brother to the root node (a new root node)
            pLastBrother = TreeList_Internal_NodeGetLastBrother(pSession->pRootNode);
            if(pLastBrother)
                pLastBrother->pBrother = pNewNode;
            else
                pSession->pRootNode->pBrother = pNewNode;
            
        }
        
        return pNewNode;
    }
    
    // Normal cases where there is a root node and we got the parent
    
    // Validate the parent integrity (NodeData crc)
    for(Nodes = 0;Nodes < pParent->NodeDataCount;Nodes++)
    {
        if(pParent->pNodeData[Nodes])
        {
            if(!TreeList_Internal_CRCCheck(pParent->pNodeData[Nodes],(sizeof(TreeListNodeData) - sizeof(pParent->pNodeData[Nodes]->CRC)),pParent->pNodeData[Nodes]->CRC))
                return 0;
        }
        
    }
    // Be the last brother of our parent's siblig
    pNewNode = TreeList_Internal_NodeCreateNew(pSession);
    if(pNewNode)
    {
        pNewNode->pParennt = pParent;
        if(pParent->pSibling) // Our parent has a sibling?
        {
            pLastBrother = TreeList_Internal_NodeGetLastBrother(pParent->pSibling); // Are there any brothers to this sibling?
            if(pLastBrother)
                pLastBrother->pBrother = pNewNode; // There are brotheres, be the last of them
            else
                pParent->pSibling->pBrother = pNewNode; // There are no brother's, be the first
        }
        else
            pParent->pSibling = pNewNode; // Be the first sibling of our parent
    }
    
    return pNewNode;
}

///////////////////////////////////////////////////////////////////////////////////////
//
// Function : TreeList_Internal_NodeFreeAllSubNodes
// Notes    : >> Recursive!! << Free all the nodes linked to a given node
// Returns  :
//
//
///////////////////////////////////////////////////////////////////////////////////////

static void TreeList_Internal_NodeFreeAllSubNodes(TreeListSession *pSession,TreeListNode *pNode)
{
  TreeListNode *pTmpNode     = 0;
  int          Node;

  if(!pSession || !pNode)
    return; // No session

  if(pNode->pSibling)
  {
    pTmpNode = pNode->pSibling;
    TreeList_Internal_NodeFreeAllSubNodes(pSession,pTmpNode);
  }

  if(pNode->pBrother)
  {
    pTmpNode = pNode->pBrother;
    TreeList_Internal_NodeFreeAllSubNodes(pSession,pTmpNode);

  }
  // Free the Nodes array
  for(Node = 0;Node < pNode->NodeDataCount;Node++)
  {
    delete pNode->pNodeData[Node];
    pNode->pNodeData[Node] = 0;
    pSession->AllocatedTreeBytes -= sizeof(TreeListNodeData);

  }

  if(pNode->pNodeData)
  {
    SAFE_DELETE(pNode->pNodeData);
    pSession->AllocatedTreeBytes -= (sizeof(TreeListNodeData*) * pSession->ColumnsCount);
  }

  free(pNode);
  pNode = 0;
  pSession->AllocatedTreeBytes -= sizeof(TreeListNode);
}

///////////////////////////////////////////////////////////////////////////////////////
//
// Function : TreeList_Internal_RectToDimensions
// Notes    : Convert MS rect to the internal TreeListDimensions struct
// Returns  :
//
//
///////////////////////////////////////////////////////////////////////////////////////

static TreeListDimensions* TreeList_Internal_RectToDimensions(RECT *pRect, TreeListDimensions *pDimensions)
{
    
    if((!pRect) || (!pDimensions))
        return 0;
    
    pDimensions->Width = pRect->right  - pRect->left;
    pDimensions->Hight = pRect->bottom - pRect->top;
    pDimensions->X     = pRect->left;
    pDimensions->Y     = pRect->top;
    
    return pDimensions;
    
}

///////////////////////////////////////////////////////////////////////////////////////
//
// Function : TreeList_Internal_AutoSetLastColumn
// Notes    : Last column to auto expand to the max width
// Returns  :
//
//
///////////////////////////////////////////////////////////////////////////////////////

static void TreeList_Internal_AutoSetLastColumn(TreeListSession *pSession)
{
    
    int     Width,NewWidth;
    int     StartPosition;
    
    if(!pSession || (pSession->ColumnsLocked == FALSE))
        return; // No session or that the headers are being updated
    
    
    pSession->ColumnDoAutoAdjust = FALSE;
    
    GetClientRect(pSession->HwndHeader, &pSession->RectHeader);
    
    // Get column width from the header control
    memset(&pSession->HeaderItem,0,sizeof(HDITEM));
    pSession->HeaderItem.mask = HDI_WIDTH;
    
    pSession->ColumnsCount = Header_GetItemCount(pSession->HwndHeader);;
    if(pSession->ColumnsCount == -1)
        return;
    
    if(Header_GetItem(pSession->HwndHeader,pSession->ColumnsCount -1,&pSession->HeaderItem) == TRUE)
    {
        Width = pSession->HeaderItem.cxy;
        StartPosition = pSession->ColumnsTotalWidth - Width;
        NewWidth = (pSession->RectHeader.right - pSession->RectHeader.left) - StartPosition + 2;
        
        memset(&pSession->HeaderItem,0,sizeof(HDITEM));
        
        pSession->HeaderItem.mask       = HDI_WIDTH;
        pSession->HeaderItem.cxy        = NewWidth;
        pSession->pColumnsInfo[(pSession->ColumnsCount -1)]->Width =  NewWidth; // Store in session
        
        Header_SetItem(pSession->HwndHeader,(pSession->ColumnsCount-1),(LPARAM)&pSession->HeaderItem);
    }
    
}

///////////////////////////////////////////////////////////////////////////////////////
//
// Function : TreeList_Internal_UpdateColumns
// Notes    :
// Returns  :
//
//
///////////////////////////////////////////////////////////////////////////////////////

static void TreeList_Internal_UpdateColumns(TreeListSession *pSession)
{
    
    int     iCol;
    HDITEM  HeaderItem;
    RECT    RectHeaderItem,RectText;
    
    if(!pSession || (pSession->ColumnsLocked == FALSE))
        return; // No session or that the headers are being updated
    
    memset(&HeaderItem,0,sizeof(HeaderItem));
    HeaderItem.mask = HDI_WIDTH;
    
    pSession->ColumnsCount = Header_GetItemCount(pSession->HwndHeader);
    if(pSession->ColumnsCount == -1)
        return;
    
    pSession->ColumnsTotalWidth = 0;
    
    // Get column widths from the header control
    for (iCol = 0; iCol < pSession->ColumnsCount; iCol++)
    {
        if(Header_GetItem(pSession->HwndHeader,iCol,&HeaderItem) == TRUE)
        {
            pSession->pColumnsInfo[iCol]->Width = HeaderItem.cxy;
            pSession->ColumnsTotalWidth += HeaderItem.cxy;
            
            if (iCol==0)
                pSession->ColumnsFirstWidth = HeaderItem.cxy;
        }
    }
    
    // Resize the editbox
    if(pSession->HwndEditBox)
    {
        
        // Get the relevant sizes
        if(Header_GetItemRect(pSession->HwndHeader,pSession->EditedColumn,&RectHeaderItem) == FALSE)
            return;
        if(TreeView_GetItemRect(pSession->HwndTreeView,pSession->EditedTreeItem,&RectText,TRUE) == FALSE)
            return;
        
        pSession->SizeEdit.Width = RectHeaderItem.right - RectHeaderItem.left;
        pSession->SizeEdit.Hight = RectText.bottom - RectText.top;
        pSession->SizeEdit.X     = RectHeaderItem.left;
        pSession->SizeEdit.Y     = RectText.top;
        
        MoveWindow(pSession->HwndEditBox,      pSession->SizeEdit.X + pSession->SizeRequested.X,                    // Position: left
            pSession->SizeEdit.Y + (RectHeaderItem.bottom - RectHeaderItem.top)+1 + pSession->SizeRequested.Y,      // Position: top
            pSession->SizeEdit.Width-1,    // Width
            pSession->SizeEdit.Hight-2,    // Height
            TRUE);
    }
}

///////////////////////////////////////////////////////////////////////////////////////
//
// Function : TreeList_Internal_RepositionControls
// Notes    : 
// Returns  :
//
//
///////////////////////////////////////////////////////////////////////////////////////
static void TreeList_Internal_RepositionControls(TreeListSession *pSession)
{
    
    DWORD dwStyle;
    int   iOffSet = 0;
    
    if(!pSession)
        return; // No session
    
    // Get Parrent Size
    GetClientRect(pSession->HwndParent, &pSession->RectParent);
    TreeList_Internal_RectToDimensions(&pSession->RectParent,&pSession->SizeParent);
    
    if(pSession->UseFullSize == TRUE)
        memcpy(&pSession->SizeRequested,&pSession->SizeParent,sizeof(TreeListDimensions)); // Full size, the control size is the same as it's parent client size
    else // Get the size of the requested rect
        TreeList_Internal_RectToDimensions(&pSession->RectRequested,&pSession->SizeRequested);
    
    // Set Header Window
    memcpy(&pSession->SizeHeader,&pSession->SizeRequested,sizeof(TreeListDimensions));
    pSession->SizeHeader.Hight = pSession->FontInfoHeader.lfHeight + TREELIST_FONT_EXTRA_HEIGHT;
    
    if(pSession->GotAnchors == TRUE)
    {
        if((pSession->CreateFlags & TREELIST_ANCHOR_RIGHT) == TREELIST_ANCHOR_RIGHT)
            pSession->SizeHeader.Width = pSession->SizeParent.Width - pSession->PointAnchors.x - pSession->RectRequested.left;
    }
    
    // Set TreeList Window size
    memcpy(&pSession->SizeTree,&pSession->SizeRequested,sizeof(TreeListDimensions));
    pSession->SizeTree.Hight = (pSession->SizeTree.Hight - pSession->SizeHeader.Hight);
    pSession->SizeTree.Y+= pSession->SizeHeader.Hight;
    
    // Use specified anchores
    if(pSession->GotAnchors == TRUE)
    {
        
        if((pSession->CreateFlags & TREELIST_ANCHOR_RIGHT) == TREELIST_ANCHOR_RIGHT)
            pSession->SizeTree.Width =  pSession->SizeParent.Width - pSession->PointAnchors.x - pSession->RectRequested.left;
        
        if((pSession->CreateFlags & TREELIST_ANCHOR_BOTTOM) == TREELIST_ANCHOR_BOTTOM)
            pSession->SizeTree.Hight =  pSession->SizeParent.Hight - (pSession->PointAnchors.y + pSession->SizeHeader.Hight) - pSession->RectRequested.top;
    }
    
    // Move Windows
    MoveWindow(pSession->HwndTreeView,TREELIST_PRNTCTLSIZE(pSession->SizeTree),1);
    MoveWindow(pSession->HwndHeader,TREELIST_PRNTCTLSIZE(pSession->SizeHeader),1);
    
    // Get the current TreeView rect
    GetClientRect(pSession->HwndTreeView, &pSession->RectTree);
    
    // Get the current Header rect
    GetClientRect(pSession->HwndHeader, &pSession->RectHeader);
    
    // Get treelist position relatve to the parent
    memcpy(&pSession->RectClientOnParent,&pSession->RectTree,sizeof(RECT));
    MapWindowPoints(pSession->HwndTreeView,pSession->HwndParent, (LPPOINT) &pSession->RectClientOnParent, 2);
    
    // Set the border rect, we should resize it as needed, so we have to map it to the parent size
    if((pSession->CreateFlags & TREELIST_DRAW_EDGE) == TREELIST_DRAW_EDGE)
    {
        
        if((pSession->UseAnchors == TRUE) && (pSession->GotAnchors == TRUE))
            InvalidateRect(pSession->HwndParent,&pSession->RectBorder,TRUE); // Delete the prev border rect
        
        
        memcpy(&pSession->RectBorder,&pSession->RectClientOnParent,sizeof(RECT));
        // Check if we have vertical scrollbars
        dwStyle = GetWindowLong(pSession->HwndTreeView, GWL_STYLE);
        if((dwStyle & WS_VSCROLL) != 0)
            iOffSet = GetSystemMetrics(SM_CXVSCROLL);
        
        pSession->RectBorder.top    =   ((pSession->RectBorder.top - pSession->SizeHeader.Hight)  - 5);
        pSession->RectBorder.left   -=  2;
        pSession->RectBorder.right  +=  2 + iOffSet;
        pSession->RectBorder.bottom +=  2;
        InvalidateRect(pSession->HwndParent,&pSession->RectBorder,FALSE);
    }
    
    // Anchors?
    if(pSession->UseAnchors == TRUE)
    {
        if(pSession->GotAnchors == FALSE)
        {
            pSession->PointAnchors.x = pSession->RectParent.right - pSession->RectClientOnParent.right;
            pSession->PointAnchors.y = pSession->RectParent.bottom - pSession->RectClientOnParent.bottom;
            pSession->GotAnchors = TRUE;
        }
    }
    
    TreeList_Internal_DestroyEditBox(pSession);         // Kill the edit box, when moving and resizing items
    TreeList_Internal_UpdateColumns(pSession);          // Update the columns sizes
    TreeList_Internal_AutoSetLastColumn(pSession);      // Set the last col to be max size
    
    
}

///////////////////////////////////////////////////////////////////////////////////////
//
// Function : TreeList_Internal_GetNodeFromTreeHandle
// Notes    : Returns the internal node pointer by the MS node handle
// Returns  :
//
//
///////////////////////////////////////////////////////////////////////////////////////

static TreeListNode *TreeList_Internal_GetNodeFromTreeHandle(TreeListSession *pSession,HTREEITEM hTreeItem)
{
  TVITEM              TreeItem;
  TreeListNode        *pNode      = 0;
  int                 Node;

  if(!pSession)
    return 0 ; // No session

  memset(&TreeItem,0,sizeof(TreeItem)); // Set all items to 0

  TreeItem.mask   = TVIF_HANDLE;
  TreeItem.hItem  = hTreeItem;

  if(TreeView_GetItem( pSession->HwndTreeView, &TreeItem ) == FALSE)
    return 0;

  pNode = (TreeListNode *)TreeItem.lParam;

  // Check the node(s) integrity
  for(Node = 0;Node < pNode->NodeDataCount;Node++)
  {
    if(pNode->pNodeData[Node])
    {
      if(!TreeList_Internal_CRCCheck(pNode->pNodeData[Node],(sizeof(TreeListNodeData) - sizeof(pNode->pNodeData[Node]->CRC)),pNode->pNodeData[Node]->CRC))
        return 0;
    }
  }

  return pNode;

}

///////////////////////////////////////////////////////////////////////////////////////
//
// Function : TreeList_Internal_DeflateRect
// Notes    : See MFC DeflateRect it's the same
// Returns  :
//
//
///////////////////////////////////////////////////////////////////////////////////////

static RECT *TreeList_Internal_DeflateRect(RECT *pRect,int left,int top,int right,int bottom)
{
    
    if(!pRect)
        return 0;
    
    pRect->bottom   -=  bottom;
    pRect->right    -=  right;
    pRect->top      +=  top;
    pRect->left     +=  left;
    
    return pRect;
    
}

///////////////////////////////////////////////////////////////////////////////////////
//
// Function : TreeList_Internal_DeflateRectEx
// Notes    : See MFC DeflateRect it's the same
// Returns  :
//
//
///////////////////////////////////////////////////////////////////////////////////////

static RECT *TreeList_Internal_DeflateRectEx(RECT *pRect,int x,int y)
{
    
    if(!pRect)
        return 0;
    
    return TreeList_Internal_DeflateRect(pRect,x,y,x,y);
}


////////////////////////////////////////////////////////////////////////////////////
//
// Function: TreeList_Internal_HandleEditBoxMessages
// Description:Subclassing the editbox to the get the key press notifications
// Parameters:
// Return:
// Logic:
//
////////////////////////////////////////////////////////////////////////////////////

static LRESULT CALLBACK TreeList_Internal_HandleEditBoxMessages (HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    
    
    TreeListSession     *pSession   = 0;
    
    pSession = (TreeListSession*)TreeList_Internal_DictGetPtr(NULL,hWnd);
    
    if(!pSession)
        return FALSE;
    
    switch (Msg)
    {
    case WM_KEYUP : // Trap the enter key (this is needed when we are working with a dialog)
        {
            if(wParam == 0x0d) // Enter key was pressed
                PostMessage(pSession->HwndParent, TREELIST_WM_EDIT_ENTER, 0, (LPARAM)pSession);
        }
        break;
    }
    
    return CallWindowProc (pSession->ProcEdit, hWnd, Msg, wParam, lParam);
}

////////////////////////////////////////////////////////////////////////////////////
//
// Function: TreeList_Internal_HandleTreeMessagesEx
// Description:Subclassing the treelist to get the scrolling notifications
// Parameters:
// Return:
// Logic:
//
////////////////////////////////////////////////////////////////////////////////////

static LRESULT CALLBACK TreeList_Internal_HandleTreeMessagesEx (HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    
    
    TreeListSession     *pSession   = 0;
    
    pSession = (TreeListSession*)TreeList_Internal_DictGetPtr(NULL,hWnd);
    if(!pSession)
        return FALSE;
    
    
    if(!pSession)
        return FALSE;
    
    switch (Msg)
    {
        
    case WM_VSCROLL : // Trap the vertical scroll
        {
            
            TreeList_Internal_DestroyEditBox(pSession); // Kill the edit box when the user scrolls vertically
        }
        break;
        
    case WM_LBUTTONUP:
        {
            pSession->ItemWasSelected = TRUE;
            
        }
        break;
    }
    
    
    return CallWindowProc (pSession->ProcTreeList, hWnd, Msg, wParam, lParam);
}


////////////////////////////////////////////////////////////////////////////////////
//
// Function:    TreeList_Internal_HandleTreeMessages
// Description: The control main message handler
// Parameters:  Windows wndProc params
// Note:        The API will subclass the parent window and this function will handle all of its messages
//
// Return:      LRESULT: FALSE if a message was not handled
//
////////////////////////////////////////////////////////////////////////////////////

static LRESULT TreeList_Internal_HandleTreeMessages (HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{


  TreeListSession     *pSession           = 0;
  TreeListDict        *pDict              = 0;
  LPNMTVCUSTOMDRAW    lpNMTVCustomDraw    = 0;
  HTREEITEM           hTreeItem           = 0;
  HDITEM              HeaderItem;
  LPNMHDR             lpNMHeader          = 0;
  LPNMHEADER          lpNMHeaderChange    = 0;
  TVHITTESTINFO       TreeTestInfo        = {0};
  TVITEM              Treeitem            = {0};
  WNDPROC             ProcParent          = (WNDPROC)GetProp(hWnd,"WNDPROC");
  RECT                RectLabel           = {0,0,0,0};
  RECT                RectText            = {0,0,0,0};
  RECT                RectItem            = {0,0,0,0};
  RECT                RectHeaderItem      = {0,0,0,0};
  RECT                RectParent          = {0,0,0,0};
  HBRUSH              brWnd               = 0;
  HBRUSH              brTextBk            = 0;
  COLORREF            clTextBk = 0,clWnd  = 0;
  HDC                 hDC                 = 0;
  HDC                 hDCMem              = 0;
  HBITMAP             hBitMapMem          = 0;
  HBITMAP             hOldBitMap          = 0;
  LPNMHEADER          pNMHeader           = 0;
  DWORD               dwMousePos          = 0;
  DWORD               dwStyle             = 0;
  BOOL                RetVal              = FALSE;
  BOOL                SelectedLine        = FALSE;
  int                 iCol                = 0;
  int                 iOffSet             = 0;
  int                 iColEnd,iColStart   = 0;
  int                 iControlID          = 0;
  TreeListSession     *pFirstSession      = 0;
  TreeListNode        *pNode              = 0;
  LPWINDOWPOS         lpPos               = 0;


  pDict = (TreeListDict*)TreeList_Internal_DictGetPtr(hWnd,NULL);
  if(!pDict)
    return FALSE;

  switch (Msg)
  {



    ///////////////////////////////////////////

  case WM_SIZE:
    {
      // Maximize
      if(wParam == SIZE_MAXIMIZED)
      {
        PostMessage(hWnd,WM_EXITSIZEMOVE,0,0);
        break;
      }
      // Restore down
      for(iControlID = 0; iControlID < TREELIST_MAX_INSTANCES;iControlID++)
      {
        pSession = pDict->pSessionPtr[iControlID];
        if(!pSession || pSession->ParentResizing == TRUE)
          break;

        PostMessage(hWnd,WM_EXITSIZEMOVE,0,0);
        break;

      }

    }
    break;
    ///////////////////////////////////////////


  case WM_ENTERSIZEMOVE:
    {
      for(iControlID = 0; iControlID < TREELIST_MAX_INSTANCES;iControlID++)
      {
        pSession = pDict->pSessionPtr[iControlID];
        if(pSession)
          pSession->ParentResizing    = TRUE;

      }
      break;

    }
    break;
    ///////////////////////////////////////////

  case WM_EXITSIZEMOVE :
    {


      for(iControlID = 0; iControlID < TREELIST_MAX_INSTANCES;iControlID++)
      {
        pSession = pDict->pSessionPtr[iControlID];

        if(!pSession)
          break;

        pSession->ParentResizing = FALSE;

        if(!pFirstSession)
          pFirstSession = pSession;

        // Check if we have to reposition the controls
        GetClientRect(pSession->HwndParent,&RectParent);
        if(memcmp(&RectParent,&pSession->RectParent,sizeof(RECT)) != 0)
        {

          if((pSession->UseFullSize == TRUE) || (pSession->UseAnchors == TRUE))
          {
            pSession->ItemWasSelected   = FALSE;
            TreeList_Internal_RepositionControls(pSession);
          }
        }
      }

      if(pFirstSession)
        InvalidateRect(pFirstSession->HwndParent,&pFirstSession->RectParent,TRUE);
    }
    break;


    ///////////////////////////////////////////

  case WM_PAINT:
    {


      for(iControlID = 0; iControlID < TREELIST_MAX_INSTANCES;iControlID++)
      {
        pSession = pDict->pSessionPtr[iControlID];
        if(!pSession || pSession->ParentResizing)
          break;

        if(pSession->ColumnDoAutoAdjust == TRUE)
          TreeList_Internal_AutoSetLastColumn(pSession);
      }

      // Double Buffering, might kill the flickering
#ifdef TREELIST_DOUBLE_BUFFEING

      hDC = BeginPaint(hWnd, &pSession->PaintStruct);
      if(hDC)
      {
        RetVal              = TRUE;
        hDCMem = CreateCompatibleDC(NULL);

        // Select the bitmap within the DC:
        hBitMapMem= CreateCompatibleBitmap(hDC, 800, 600);
        hOldBitMap = SelectObject(hDCMem, hBitMapMem);
        GetClientRect(pSession->HwndTreeView,&pSession->RectTree);
        BitBlt(hDC, 0, 0, pSession->RectTree.right, pSession->RectTree.bottom, hDCMem, 0, 0, SRCCOPY);
        SelectObject(hDCMem, hOldBitMap);

        DeleteObject(hBitMapMem);
        DeleteDC(hDCMem);
        EndPaint(hWnd, &pSession->PaintStruct);

      }
#endif

    }
    break;


    ///////////////////////////////////////////
  case    WM_CTLCOLOREDIT:
    {


      for(iControlID = 0; iControlID < TREELIST_MAX_INSTANCES;iControlID++)
      {
        pSession = pDict->pSessionPtr[iControlID];
        if(!pSession || pSession->ParentResizing)
          break;
        if( ((HWND)lParam==pSession->HwndEditBox) && (pSession->EditBoxStyleNormal == FALSE))
        {
          RetVal              = TRUE;
          SetTextColor((HDC)wParam,   RGB(255,255,255));
          SetBkColor((HDC)wParam, RGB(0,0,0));
        }
      }
    }
    break;

    ///////////////////////////////////////////

  case TREELIST_WM_EDIT_NODE:
    {

      pSession = (TreeListSession*)lParam;

      if(!pSession || pSession->ParentResizing)
        break;

      if(pSession->HwndEditBox)
        break;


      RetVal              = TRUE;
      pNode = TreeList_Internal_GetNodeFromTreeHandle(pSession,pSession->EditedTreeItem);
      if(pNode)
      {
        // This is place to edit a cell that needs editing
        if(Header_GetItemRect(pSession->HwndHeader,pSession->EditedColumn,&RectHeaderItem) == FALSE)
        {
          pSession->EditedColumn = 0;
          pSession->EditedTreeItem = 0;
          break;
        }
        if(TreeView_GetItemRect(pSession->HwndTreeView,pSession->EditedTreeItem,&RectText,TRUE) == FALSE)
        {
          pSession->EditedColumn = 0;
          pSession->EditedTreeItem = 0;
          break;
        }
        pSession->SizeEdit.Width = RectHeaderItem.right - RectHeaderItem.left;
        pSession->SizeEdit.Hight = RectText.bottom - RectText.top;
        pSession->SizeEdit.X     = RectHeaderItem.left;
        pSession->SizeEdit.Y     = RectText.top;

        // Check if we have vertical scrollbars
        // Only if we are editing the last caolumn (4/26/20110)
        iOffSet = 0;
        if(pSession->EditedColumn == (pSession->ColumnsCount -1))
        {
          dwStyle = GetWindowLong(pSession->HwndTreeView, GWL_STYLE);
          if((dwStyle & WS_VSCROLL) != 0)
            iOffSet = GetSystemMetrics(SM_CXVSCROLL);
        }
        // a Numeric only text box
        if(pNode->pNodeData[pSession->EditedColumn]->Numeric == TRUE)
          dwStyle = WS_CHILD|WS_VISIBLE|ES_NUMBER;
        else
          dwStyle = WS_CHILD|WS_VISIBLE;

        pSession->HwndEditBox = CreateWindowEx(0,
          "EDIT",
          pNode->pNodeData[pSession->EditedColumn]->Data.c_str(),
          dwStyle,
          pSession->SizeEdit.X + pSession->SizeRequested.X,                                                       // Position: left
          pSession->SizeEdit.Y + ((RectHeaderItem.bottom - RectHeaderItem.top)) + pSession->SizeRequested.Y,    // Position: top
          pSession->SizeEdit.Width-1-iOffSet,    // Width
          pSession->SizeEdit.Hight-2,            // Height
          pSession->HwndParent,                  // Parent window handle
          0,
          pSession->InstanceParent, 0);

        if(pSession->HwndEditBox )
        {

          TreeList_Internal_DictUpdate(FALSE,pSession,pSession->HwndParent,pSession->HwndEditBox);
          pSession->ProcEdit = (WNDPROC)SetWindowLong(pSession->HwndEditBox, GWL_WNDPROC, (LONG)TreeList_Internal_HandleEditBoxMessages); // Subclassing the control (it will help trapping the enter key press event on a dialog)
          SendMessage(pSession->HwndEditBox, WM_SETFONT, (WPARAM)pSession->FontHandleEdit, (LPARAM)TRUE);
          SetFocus(pSession->HwndEditBox);
          size_t len = strlen(pNode->pNodeData[pSession->EditedColumn]->Data.c_str());
          SendMessage(pSession->HwndEditBox,EM_SETSEL,len, len); // Select all text

        }
        else
        {
          pSession->EditedColumn = 0;
          pSession->EditedTreeItem = 0;
        }

      }

    }
    break;
    ///////////////////////////////////////////
  case TREELIST_WM_EDIT_ENTER:
    {

      pSession = (TreeListSession*)lParam;
      if(!pSession || pSession->ParentResizing)
        break;

      if(pSession->WaitingForCaller == TRUE)// Do nothong while waiting for the caller
        break;

      pNode = TreeList_Internal_GetNodeFromTreeHandle(pSession,pSession->EditedTreeItem);
      if(!pNode)
        break;

      RetVal              = TRUE;
      memset(pSession->EditBoxBuffer,0,sizeof(pSession->EditBoxBuffer));
      if(GetWindowText(pSession->HwndEditBox,pSession->EditBoxBuffer,TREELIST_MAX_STRING) > 0)// Get edit box text
      {
        if(strcmp(pSession->EditBoxBuffer,pNode->pNodeData[pSession->EditedColumn]->Data.c_str()) != 0)
        {
          // Call the user!
          if(pSession->pCBValidateEdit)
          {
            pSession->WaitingForCaller = TRUE;
            pSession->EditBoxOverrideBuffer[0] = 0;

            if(pSession->pCBValidateEdit((NODE_HANDLE)pSession,pNode->pNodeData[pSession->EditedColumn]->pExternalPtr,pSession->EditBoxBuffer, pSession->EditBoxOverrideBuffer) == TRUE)
            {

              if( pSession->EditBoxOverrideBuffer[0])
                //reset(pNode->pNodeData[pSession->EditedColumn]->Data, new string(pSession->EditBoxOverrideBuffer));
                pNode->pNodeData[pSession->EditedColumn]->Data = pSession->EditBoxOverrideBuffer;
              else
                pNode->pNodeData[pSession->EditedColumn]->Data = pSession->EditBoxBuffer;
              //reset(pNode->pNodeData[pSession->EditedColumn]->Data, new string(pSession->EditBoxBuffer));
              pNode->pNodeData[pSession->EditedColumn]->Altered = TRUE; // Set "changed" flag so we will be able to color it later
              // Update CRC
              pNode->pNodeData[pSession->EditedColumn]->CRC = TreeList_Internal_CRCCreate(pNode->pNodeData[pSession->EditedColumn],
                (sizeof(TreeListNodeData) - sizeof(pNode->pNodeData[pSession->EditedColumn]->CRC))); // Set the srs sig
            }
            pSession->WaitingForCaller = FALSE;
          }
        }
      }


      TreeList_Internal_DestroyEditBox(pSession);// Kill the edit box

      // Select the edited line in the Tree
      if(pNode && pSession->EditedColumn > 0)
        TreeView_SelectItem(pSession->HwndTreeView,pNode->TreeItemHandle);

      SetFocus(pSession->HwndTreeView);
      pSession->EditedColumn      = 0;
      pSession->EditedTreeItem    = 0;
      break;

    }
    break;
    ///////////////////////////////////////////
  case WM_NOTIFY:
    {


      lpNMHeader = (LPNMHDR)lParam;
      pSession = (TreeListSession*)TreeList_Internal_DictGetPtr(hWnd,lpNMHeader->hwndFrom);
      if(!pSession || pSession->ParentResizing)
        break;


      switch(lpNMHeader->code)
      {


        ///////////////////////////////////////////
      case NM_CUSTOMDRAW:
        {

          RetVal              = TRUE;
          lpNMTVCustomDraw    = (LPNMTVCUSTOMDRAW) lParam;

          if(lpNMHeader->hwndFrom != pSession->HwndTreeView)
            break;

          switch (lpNMTVCustomDraw->nmcd.dwDrawStage)
          {

          case CDDS_PREPAINT:
            return CDRF_NOTIFYITEMDRAW;

          case CDDS_ITEMPREPAINT:
            return (CDRF_DODEFAULT | CDRF_NOTIFYPOSTPAINT);

          case CDDS_ITEMPOSTPAINT:
            {
              // Get a valid pinter to our internal data type
              hTreeItem = (HTREEITEM)lpNMTVCustomDraw->nmcd.dwItemSpec;
              pNode = TreeList_Internal_GetNodeFromTreeHandle(pSession,hTreeItem);
              if(!pNode || pNode->NodeDataCount == 0)
                return(CDRF_DODEFAULT);

              memcpy(&RectItem,&lpNMTVCustomDraw->nmcd.rc,sizeof(RECT));
              if (IsRectEmpty(&RectItem) == TRUE)
              {
                // Nothing to paint when we have an empty rect
                SetWindowLong(pSession->HwndParent, DWL_MSGRESULT, CDRF_DODEFAULT);
                break;
              }

              hDC = lpNMTVCustomDraw->nmcd.hdc;
              if(!hDC)
                return(CDRF_DODEFAULT); // No HDC

              SetBkMode(hDC, TRANSPARENT);

              if(TreeView_GetItemRect(pSession->HwndTreeView,hTreeItem,&RectLabel,TRUE) == FALSE)
                return(CDRF_DODEFAULT); // No RECT

              clTextBk    = lpNMTVCustomDraw->clrTextBk;
              clWnd       = TreeView_GetBkColor(pSession->HwndTreeView);
              //brTextBk    = CreateSolidBrush(clTextBk);
              TreeListNodeData *data = pNode->pNodeData[0];
              if (data->Colored) {
                brTextBk    = CreateSolidBrush(data->Colored ? data->BackgroundColor : clTextBk);
                SetTextColor(hDC,RGB(255, 255, 255));
              } else {
                brTextBk    = CreateSolidBrush(clTextBk);
                SetTextColor(hDC,RGB(0,0,0)); // Make sure we use black color
              }
              brWnd       = CreateSolidBrush(clWnd);

              // Clear the original label rectangle
              RectLabel.right = pSession->RectTree.right;
              //FillRect(hDC,&RectLabel,brWnd);
              FillRect(hDC,&RectLabel,brTextBk);

              pSession->ColumnsCount = Header_GetItemCount(pSession->HwndHeader);
              if(pSession->ColumnsCount == -1)
                return(CDRF_DODEFAULT); // No columns info, nothing to do

              // Draw the horizontal lines
              for (iCol =0; iCol <  pSession->ColumnsCount ; iCol++)
              {
                // Get current columns width from the header window
                memset(&HeaderItem,0,sizeof(HeaderItem));
                HeaderItem.mask = HDI_HEIGHT | HDI_WIDTH;
                if(Header_GetItem(pSession->HwndHeader,iCol,&HeaderItem) == TRUE)
                {
                  pSession->pColumnsInfo[iCol]->Width = HeaderItem.cxy;
                  iOffSet +=  HeaderItem.cxy ;
                  RectItem.right = iOffSet - 1;
                  DrawEdge(hDC,&RectItem, BDR_SUNKENINNER,BF_RIGHT);
                }
              }

              // Draw the vertical lines
              DrawEdge(hDC,&RectItem, BDR_SUNKENINNER, BF_BOTTOM);

              // Draw Label, calculate the rect first
              const char *str = pNode->pNodeData[0]->Data.c_str();
              DrawText(hDC,str, strlen(str),&RectText,DT_NOPREFIX | DT_CALCRECT);
              RectLabel.right = TREELIST_MIN( (RectLabel.left + RectText.right + 4), pSession->pColumnsInfo[0]->Width  - 4);

              if ((RectLabel.right - RectLabel.left) < 0)
                brTextBk = brWnd;

              if (clTextBk != clWnd)  // Draw label's background
              {

                if(pSession->ItemWasSelected == TRUE)
                {
                  SelectedLine = TRUE;
                  SetTextColor(hDC,RGB(255,255,255));
                  RectLabel.right = pSession->RectTree.right;
                  FillRect(hDC,&RectLabel, brTextBk);   
                } 
              }

              // Draw main label
              memcpy(&RectText,&RectLabel,sizeof(RECT));

              // The label right shoud be as the column right
              if(Header_GetItemRect(pSession->HwndHeader,0,&RectHeaderItem) == FALSE)
                return(CDRF_DODEFAULT);// Error getting the rect


              RectText.right = RectHeaderItem.right; // Set the right side
              TreeList_Internal_DeflateRectEx(&RectText,2, 1); // Defalate it
              string *s = &pNode->pNodeData[0]->Data;
              DrawText(hDC,s->c_str(),strlen(s->c_str()), &RectText, DT_NOPREFIX | DT_END_ELLIPSIS); // Draw it
              iOffSet =pSession->pColumnsInfo[0]->Width;

              // Draw thwe other labels (the columns)
              for (iCol =1; iCol < pSession->ColumnsCount; iCol++)
              {

                if(pNode->pNodeData[iCol])
                {
                  memcpy(&RectText,&RectLabel,sizeof(RECT));
                  RectText.left = iOffSet;
                  RectText.right = iOffSet + pSession->pColumnsInfo[iCol]->Width;

                  // Set cell bk color
                  if((SelectedLine == FALSE) && (pNode->pNodeData[iCol]->Colored == TRUE))
                  {
                    memcpy(&RectLabel,&RectText,sizeof(RECT));
                    if(brTextBk)
                      DeleteObject(brTextBk);
                    brTextBk    = CreateSolidBrush(pNode->pNodeData[iCol]->BackgroundColor);
                    RectLabel.top       += 1;
                    RectLabel.bottom    -= 1;
                    RectLabel.right     -= 2;
                    FillRect(hDC,&RectLabel,brTextBk);

                  }


                  TreeList_Internal_DeflateRect(&RectText,TREELIST_FONT_TEXT_CELL_OFFSET, 1, 2, 1); // This is an "MFC" remake thing :)

                  if(pNode->pNodeData[iCol])
                  {

                    // Set specific text color (only when this is not the selected line)
                    if(SelectedLine == FALSE)
                    {
                      if(pNode->pNodeData[iCol]->Colored == TRUE)
                        SetTextColor(hDC,pNode->pNodeData[iCol]->TextColor);

                      // Set special color for altered cells (if set by ty the caller)
                      if(pNode->pNodeData[iCol]->Altered == TRUE)
                        SetTextColor(hDC,pNode->pNodeData[iCol]->AltertedTextColor);

                    }

                    const char *str = pNode->pNodeData[iCol]->Data.c_str();
                    DrawText(hDC,str,strlen(str), &RectText, DT_NOPREFIX | DT_END_ELLIPSIS);
                  }
                  iOffSet += pSession->pColumnsInfo[iCol]->Width;
                }
              }

              SetTextColor(hDC,RGB(0,0,0));

              // Draw the rect (on the parent) around the tree
              if((pSession->CreateFlags & TREELIST_DRAW_EDGE) == TREELIST_DRAW_EDGE)
                DrawEdge(GetDC(pSession->HwndParent),&pSession->RectBorder, EDGE_ETCHED,BF_RECT);

              return(CDRF_DODEFAULT);
            }
            break;
          }
        }
        break;

        ///////////////////////////////////////////
      case NM_RELEASEDCAPTURE:
        {


          RetVal              = TRUE;
          TreeList_Internal_DestroyEditBox(pSession); // Kill the edit box, when resizing a column

          TreeList_Internal_UpdateColumns(pSession);
          pSession->ColumnDoAutoAdjust = TRUE;

          // Refresh
          InvalidateRect(pSession->HwndParent,&pSession->RectClientOnParent,TRUE);



        }
        break;

        ///////////////////////////////////////////
      case NM_CLICK:
        {

          if(lpNMHeader->hwndFrom != pSession->HwndTreeView)
            break;

          if(pSession->WaitingForCaller == TRUE)// Do nothong while waiting for the caller
            break;

          TreeList_Internal_DestroyEditBox(pSession);// Kill the edit box

          RetVal              = FALSE;  // Will make our control respond to click same as dblcick
          dwMousePos          = GetMessagePos();
          TreeTestInfo.pt.x   = GET_X_LPARAM(dwMousePos);
          TreeTestInfo.pt.y   = GET_Y_LPARAM(dwMousePos);
          MapWindowPoints(HWND_DESKTOP, lpNMHeader->hwndFrom, &TreeTestInfo.pt, 1);
          TreeView_HitTest(lpNMHeader->hwndFrom, &TreeTestInfo);

          if(TreeTestInfo.hItem)
          {
            pNode = TreeList_Internal_GetNodeFromTreeHandle(pSession,TreeTestInfo.hItem);
            if(!pNode)
              break;

            pSession->ItemWasSelected = TRUE;
            InvalidateRect(pSession->HwndTreeView,&pSession->RectTree,FALSE);   

            // Get the correct column where the mouse has clicked..
            iColStart = pSession->RectHeader.left;
            iColEnd   = 0;
            for(iCol = 0;iCol < pSession->ColumnsCount; iCol++)
            {
              iColEnd += pSession->pColumnsInfo[iCol]->Width;

              if((TreeTestInfo.pt.x >= iColStart) && (TreeTestInfo.pt.x < iColEnd))
              {
                if(pNode->NodeDataCount >= iCol) // Is there any data there?
                {
                  if(pNode->pNodeData[iCol] && pNode->pNodeData[iCol]->Editable == TRUE) // Is it editable?
                  {

                    // Send edit message to the parent window
                    pSession->EditedColumn = iCol;
                    pSession->EditedTreeItem = TreeTestInfo.hItem;
                    PostMessage(pSession->HwndParent, TREELIST_WM_EDIT_NODE, 0, (LPARAM)pSession);
                    break;
                  }
                }
              }
              iColStart = iColEnd;
            }

            if(pSession->HwndEditBox)
              SendMessage(pSession->HwndParent, TREELIST_WM_EDIT_ENTER, 0, 0);
            else
            {
              if(iCol > 0)
                TreeView_SelectItem(lpNMHeader->hwndFrom,TreeTestInfo.hItem);
            }
          }
        }

        break;
      }
    }
  }

  if(hDC) // Cleanup
  {

    if(brWnd)
      DeleteObject(brWnd);

    if(brTextBk)
      DeleteObject(brTextBk);

  }

  if(RetVal == FALSE)
  {
    if(ProcParent)
      return CallWindowProc(ProcParent, hWnd, Msg, wParam, lParam);
    else
      return FALSE;
  }
  else
    return RetVal;

}

///////////////////////////////////////////////////////////////////////////////////////
//
//
// API
//
//
///////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////
//
// Function:    TreeListCreate
// Description: Init the TreeList API, Allocates memory and create the internal data types
// Parameters:  HINSTANCE           Applicayion instance
//              HWND                Parent window handler
//              RECT*               a rect for a fixed size control, NULL will cause the control to ne 100% of
//                                  It's parent size
//              DWORD               Creation flags
//              TREELIST_CB*        a pointer to a call back to validate the user edit requests (can be NULL)
// Return:      TREELIST_HANDLE     a valid handle to the listtree control
//
////////////////////////////////////////////////////////////////////////////////////

TREELIST_HANDLE TreeListCreate(HINSTANCE Instance, HWND Hwnd,RECT *pRect,DWORD dwFlags, TREELIST_CB *pFunc)
{
    
    TreeListSession     *pSession       = 0;
    BOOL                Error           = FALSE;
    BOOL                PrevInstance    = FALSE;
    
    if((Hwnd == 0) || (Instance == 0))
        return TREELIST_INVLID_HANDLE;
    
    // Couldn't make VC2010 link with RemoveProp, didn't have the time to figure out why, so..
#if _MSC_VER > 1200
    if(!hDllHandle)
    {
        hDllHandle = LoadLibrary("user32.dll");
        if (hDllHandle != NULL)
        {
            pRemoveProp = (LPREMOVEPROP)GetProcAddress(hDllHandle,"RemovePropA");
            if (!pRemoveProp)
            {
                // handle the error
                FreeLibrary(hDllHandle);
                return TREELIST_INVLID_HANDLE;
            }
        }
    }
#endif
    
    pSession = (TreeListSession*)malloc(sizeof(TreeListSession));
    if(!pSession)
        return TREELIST_INVLID_HANDLE;
    
    memset(pSession,0,sizeof(TreeListSession));
    pSession->AllocatedTreeBytes = sizeof(TreeListSession);
    
    pSession->InstanceParent    = Instance;
    pSession->HwndParent        = Hwnd;
    
    // This essentoal for the tree view
    
    // Do we have a prev instance?
    if(TreeList_Internal_DictGetPtr(Hwnd,NULL))
        PrevInstance = TRUE;
    
    if(PrevInstance == FALSE)
        InitCommonControls();
    do
    {
        
        // Create a font for the TreeList control
        pSession->FontHandleTreeList = CreateFont(14, 0, 0, 0, 500, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_CHARACTER_PRECIS, PROOF_QUALITY,
            FF_DONTCARE, "Courier");
        
        // Create a font for the Header control
        pSession->FontHandleHeader = CreateFont(16, 0, 0, 0, 700, FALSE, FALSE, FALSE, // 700 = Bold font
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_CHARACTER_PRECIS, PROOF_QUALITY,
            FF_DONTCARE, "Arial");
        
        // Create a font for the edit box
        pSession->FontHandleEdit = CreateFont(14, 0, 0, 0, 500, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_CHARACTER_PRECIS, PROOF_QUALITY,
            FF_DONTCARE, "Courier");
        
        
        // Check that we got the fonts
        if((pSession->FontHandleTreeList == NULL) || (pSession->FontHandleHeader == NULL) || (pSession->FontHandleEdit == NULL))
        {
            Error = TRUE;
            break;
        }
        // Get the fonts info
        GetObject(pSession->FontHandleTreeList, sizeof(LOGFONT), &pSession->FontInfoTreeList);
        GetObject(pSession->FontHandleHeader,   sizeof(LOGFONT), &pSession->FontInfoHeader);
        GetObject(pSession->FontHandleEdit,     sizeof(LOGFONT), &pSession->FontInfoEdit);
        
        if(pRect) // User provided a control rect
            memcpy(&pSession->RectRequested,pRect,sizeof(RECT));
        
        else
            pSession->UseFullSize = TRUE;
        
        
        // Create the list View and the header
        pSession->HwndTreeView = CreateWindowEx(0,                                  // Extended styles
            WC_TREEVIEW,                                                            // Control 'class' name
            0,                                                                      // Control caption
            WS_CHILD | WS_VISIBLE | TVS_FULLROWSELECT | TVS_NOHSCROLL | TVS_NOTOOLTIPS | TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT,
            0,0,0,0,                                                                // Position
            pSession->HwndParent,                                                   // Parent window handle
            0,                                                                      // Control's ID
            pSession->InstanceParent,                                               // Instance
            0);                                                                     // User defined info
        
        if(!pSession->HwndTreeView)
        {
            Error = TRUE;
            break;
        }
        
        
        TreeList_Internal_DictUpdate(FALSE,pSession,pSession->HwndParent,pSession->HwndTreeView);
        SendMessage(pSession->HwndTreeView, WM_SETFONT, (WPARAM)pSession->FontHandleTreeList, (LPARAM)TRUE);
        pSession->ProcTreeList = (WNDPROC)SetWindowLong(pSession->HwndTreeView, GWL_WNDPROC, (LONG)TreeList_Internal_HandleTreeMessagesEx); // Sub classing the control
        
        // Sub class the parent window
        if(PrevInstance == FALSE)
        {
            pSession->ProcParent = (WNDPROC)SetWindowLong(pSession->HwndParent, GWL_WNDPROC, (LONG)TreeList_Internal_HandleTreeMessages); // Sub classing the control
            SetProp(pSession->HwndParent,"WNDPROC",pSession->ProcParent);
        }
        
        pSession->HwndHeader = CreateWindowEx(0,                                    // Extended styles
            WC_HEADER,                                                              // Control 'class' name
            0,                                                                      // Control caption
            WS_CHILD | WS_VISIBLE | HDS_FULLDRAG,                                   // Win style
            0,0,0,0,                                                                // Position
            pSession->HwndParent,                                                   // Parent window handle
            0,                                                                      // Control's ID
            pSession->InstanceParent,                                               // Instance
            0);                                                                     // User defined info
        
        if(!pSession->HwndTreeView)
        {
            Error = TRUE;
            break;
        }
        
        TreeList_Internal_DictUpdate(FALSE,pSession,pSession->HwndParent,pSession->HwndHeader);
        SendMessage(pSession->HwndHeader, WM_SETFONT, (WPARAM)pSession->FontHandleHeader, (LPARAM)TRUE);
        
    }
    while(0);
    
    if(Error == TRUE)
    {
        if(pSession)
        {
            if(pSession->FontHandleTreeList)
                DeleteObject(pSession->FontHandleTreeList);
            if(pSession->FontHandleHeader)
                DeleteObject(pSession->FontHandleHeader);
            if(pSession->FontHandleEdit)
                DeleteObject(pSession->FontHandleEdit);
            free(pSession);
        }
        return TREELIST_INVLID_HANDLE;
        
    }
    
    if(pFunc)
        pSession->pCBValidateEdit = pFunc;
    
    pSession->CreateFlags       = dwFlags;
    if(((pSession->CreateFlags & TREELIST_ANCHOR_RIGHT) == TREELIST_ANCHOR_RIGHT) || ((pSession->CreateFlags & TREELIST_ANCHOR_BOTTOM) == TREELIST_ANCHOR_BOTTOM))
        pSession->UseAnchors = TRUE;
    
    // Edit box style
    if((pSession->CreateFlags & TREELIST_NORMAL_EDITBOX) == TREELIST_NORMAL_EDITBOX)
        pSession->EditBoxStyleNormal = TRUE;

    TreeView_SetBkColor(pSession->HwndTreeView,RGB(255,255,255));
    TreeView_SetTextColor(pSession->HwndTreeView,RGB(0,0,0));
    TreeList_Internal_RepositionControls(pSession);
    
    return (TREELIST_HANDLE)pSession;
}


////////////////////////////////////////////////////////////////////////////////////
//
// Function:    TreeListDestroy
// Description: Delete all elements and free memory
// Parameters:  TREELIST_HANDLE a valid handle to a listtree control
// Note:        void
//
//
////////////////////////////////////////////////////////////////////////////////////

int  TreeListDestroy         (TREELIST_HANDLE ListTreeHandle)
{
    
    int iCol;
    int AllocatedBytes;
    TreeListSession *pSession = (TreeListSession*)ListTreeHandle;
    
    if(!pSession)
        return e_ERROR_NO_SESSION;
    
    // Kill windows objects
    if(pSession->FontHandleTreeList)
        DeleteObject(pSession->FontHandleTreeList);
    if(pSession->FontHandleHeader)
        DeleteObject(pSession->FontHandleHeader);
    if(pSession->FontHandleEdit)
        DeleteObject(pSession->FontHandleEdit);
    
    
    TreeList_Internal_DestroyEditBox(pSession); // Kill the edit box
    
    // Kill the header window
    if(pSession->HwndHeader)
    {
        TreeList_Internal_DictUpdate(TRUE,pSession,NULL,pSession->HwndHeader);
        DestroyWindow(pSession->HwndHeader);
        
    }
    
    // Kill the TreeView main window
    if(pSession->HwndTreeView)
    {
        TreeList_Internal_DictUpdate(TRUE,pSession,NULL,pSession->HwndTreeView);
        (WNDPROC)SetWindowLong(pSession->HwndParent, GWL_WNDPROC, (LONG)pSession->ProcParent); // Restore the original wnd proc to the parent
        pRemoveProp(pSession->HwndParent,"WNDPROC");
        DestroyWindow(pSession->HwndTreeView);;
    }
    
    // Free all the columns
    for(iCol = 0;iCol < pSession->ColumnsCount;iCol++)
    {
        if(pSession->pColumnsInfo[iCol])
        {
            free(pSession->pColumnsInfo[iCol]);
            pSession->pColumnsInfo[iCol] = 0;
            pSession->AllocatedTreeBytes -= sizeof(TreeListColumnInfo);
        }
    }
    
    if(pSession->pColumnsInfo)
    {
        free(pSession->pColumnsInfo);
        pSession->AllocatedTreeBytes -= ((TREELIST_MAX_COLUMNS +1) * sizeof(TreeListColumnInfo*));
        
    }
    
    // Free all the nodes
    TreeList_Internal_NodeFreeAllSubNodes(pSession, pSession->pRootNode);
    AllocatedBytes = pSession->AllocatedTreeBytes;
    InvalidateRect(pSession->HwndParent,&pSession->RectParent,TRUE);
    free(pSession);
    pSession = 0;
    
    // At this point we can expect that this little counter would be 0
    AllocatedBytes -= sizeof(TreeListSession);
    
    return AllocatedBytes; //Should be e_OK == 0!
}

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

TreeListError TreeListAddColumn(TREELIST_HANDLE ListTreeHandle,char *szColumnName,int Width)
{
    TreeListSession *pSession = (TreeListSession*)ListTreeHandle;
    
    if(!pSession)
        return e_ERROR_NO_SESSION;
    
    if(pSession->ColumnsLocked == TRUE)
        return e_ERROR_COULD_NOT_ADD_COLUMN;
    
    if(pSession->ColumnsCount >= TREELIST_MAX_COLUMNS)
        return e_ERROR_COULD_NOT_ADD_COLUMN;
    
    // Allocate the pointers array
    if(!pSession->pColumnsInfo)
    {
        pSession->pColumnsInfo = (TreeListColumnInfo**)malloc((TREELIST_MAX_COLUMNS +1) * sizeof(TreeListColumnInfo*));
        if(!pSession->pColumnsInfo)
            return e_ERROR_MEMORY_ALLOCATION;
        
        pSession->AllocatedTreeBytes += ((TREELIST_MAX_COLUMNS +1) * sizeof(TreeListColumnInfo*));
        
    }
    
    pSession->pColumnsInfo[pSession->ColumnsCount] = (TreeListColumnInfo*)malloc(sizeof(TreeListColumnInfo));
    if(!pSession->pColumnsInfo[pSession->ColumnsCount])
        return e_ERROR_MEMORY_ALLOCATION;
    
    pSession->AllocatedTreeBytes += sizeof(TreeListColumnInfo);
    memset(pSession->pColumnsInfo[pSession->ColumnsCount],0,sizeof(TreeListColumnInfo));
    strncpy(pSession->pColumnsInfo[pSession->ColumnsCount]->ColumnName,szColumnName,TREELIST_MAX_STRING);
    pSession->pColumnsInfo[pSession->ColumnsCount]->Width = Width;
    
    memset(&pSession->HeaderItem,0,sizeof(HDITEM));
    pSession->HeaderItem.mask       = HDI_WIDTH | HDI_FORMAT|HDI_TEXT;
    pSession->HeaderItem.cxy        = Width;
    
    if(Width == TREELIST_LAST_COLUMN)
    {
        pSession->ColumnsLocked         = TRUE;
        pSession->HeaderItem.cxy        = 100;
        pSession->pColumnsInfo[pSession->ColumnsCount]->Width = pSession->HeaderItem.cxy ;
        
    }
    
    pSession->HeaderItem.fmt        = HDF_CENTER;
    pSession->HeaderItem.pszText    = pSession->pColumnsInfo[pSession->ColumnsCount]->ColumnName;
    pSession->HeaderItem.cchTextMax = strlen(pSession->pColumnsInfo[pSession->ColumnsCount]->ColumnName);
    pSession->ColumnsTotalWidth     += pSession->HeaderItem.cxy;
    Header_InsertItem(pSession->HwndHeader,pSession->ColumnsCount,(LPARAM)&pSession->HeaderItem);
    
    pSession->ColumnsCount++;
    pSession->pColumnsInfo[pSession->ColumnsCount] = 0; // Null the next member
    
    if(pSession->ColumnsLocked == TRUE)
        TreeList_Internal_AutoSetLastColumn(pSession);
    
    return e_ERROR_COULD_NOT_ADD_COLUMN;
}

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

NODE_HANDLE TreeListAddNode(TREELIST_HANDLE ListTreeHandle,NODE_HANDLE ParentHandle,TreeListNodeData *RowOfColumns,int ColumnsCount)
{
    
    TreeListSession     *pSession       = (TreeListSession*)ListTreeHandle;
    TreeListNode        *pNewNode       = 0;
    TreeListNode        *pParentNode    = (TreeListNode*)ParentHandle;
    TVITEM              TreeItem;
    int                 Node;
    
    if(!pSession)
        return 0; // Null handle
    
    if(!RowOfColumns || ColumnsCount== 0)// No data to add
        return 0;
    
    pSession->ColumnsLocked = TRUE; // Lock columns
    
    pNewNode = TreeList_Internal_AddNode(pSession,pParentNode);
    if(pNewNode)
    {
        for(Node = 0;Node < ColumnsCount;Node++)
        {
            if(!TreeList_Internal_NodeColonize(pSession,pNewNode,RowOfColumns + Node))
                return 0; // Could not add the columns data
        }
        
        // Update UI Properties
        TreeItem.mask        = TVIF_TEXT | TVIF_PARAM;
        TreeItem.pszText     = (LPSTR)strdup(pNewNode->pNodeData[0]->Data.c_str());
        TreeItem.cchTextMax  = strlen(pNewNode->pNodeData[0]->Data.c_str());
        TreeItem.lParam      = (LPARAM)pNewNode;
        
        // Updatge the base struct
        pSession->TreeStruct.item           = TreeItem;
        pSession->TreeStruct.hInsertAfter   = 0;
        if(!pParentNode)
            pSession->TreeStruct.hParent        = TVI_ROOT;
        else
            pSession->TreeStruct.hParent        = pParentNode->TreeItemHandle;
        
        
        pNewNode->TreeItemHandle =  (HTREEITEM)TreeView_InsertItem(pSession->HwndTreeView,&pSession->TreeStruct);
        if(!pNewNode->TreeItemHandle) // Error so ..
        {
            free(pNewNode);
            pNewNode = 0;
        }
    }
    
    return (NODE_HANDLE)pNewNode;
}

////////////////////////////////////////////////////////////////////////////////////
