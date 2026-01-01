#include "drag_drop.h"

#include <stdio.h>
#include <stdbool.h>

#if defined(_WIN32)

#include <Windows.h>
#include <ole2.h>
#include <strsafe.h>
#include <shlobj.h>
#include <process.h>

#include "common.h"

static bool initialized;

struct file_object
{
    IDataObject IDataObject_iface;
    LONG ref_count;
    char* file_path;
};

static HRESULT STDMETHODCALLTYPE Data_QueryInterface(IDataObject* this, REFIID riid, void** ppv)
{
    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &IID_IDataObject)) {
        *ppv = this;
		this->lpVtbl->AddRef(this);
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE Data_AddRef(IDataObject* this)
{
    struct file_object* obj = (struct file_object*)this;
    return InterlockedIncrement(&obj->ref_count);
}

static ULONG STDMETHODCALLTYPE Data_Release(IDataObject* this)
{
    struct file_object* obj = (struct file_object*)this;
    ULONG ref = InterlockedDecrement(&obj->ref_count);
    if (!ref)
    {
        free(obj->file_path);
        free(obj);
    }
    return ref;
}

static HRESULT STDMETHODCALLTYPE Data_GetData(IDataObject* this, FORMATETC* fmt, STGMEDIUM* med)
{
    if (!fmt || !med)
        return E_INVALIDARG;

    if (fmt->cfFormat != CF_HDROP ||
		!(fmt->tymed & TYMED_HGLOBAL) ||
		fmt->dwAspect != DVASPECT_CONTENT ||
		fmt->lindex != -1 ||
		fmt->ptd != NULL)
        return DV_E_FORMATETC;

    char* path = ((struct file_object*)this)->file_path;
	int path_length = string_length(path);

    HGLOBAL hMem = GlobalAlloc(GHND, sizeof(DROPFILES) + path_length + 2);
    if (!hMem)
        return E_OUTOFMEMORY;

    DROPFILES* df = (DROPFILES*)GlobalLock(hMem);
    if (!df)
    {
        GlobalFree(hMem);
        return E_OUTOFMEMORY;
    }

    df->pFiles = sizeof(DROPFILES);
    df->pt.x = 0;
    df->pt.y = 0;
    df->fNC = FALSE;
    df->fWide = FALSE;

	char* dest = (char*)df + sizeof(DROPFILES);
	memcpy(dest, path, path_length);
	dest[path_length] = '\0';
	dest[path_length + 1] = '\0';

    GlobalUnlock(hMem);

    med->tymed = TYMED_HGLOBAL;
    med->hGlobal = hMem;
    med->pUnkForRelease = NULL;

    return S_OK;
}

static HRESULT STDMETHODCALLTYPE Data_GetDataHere() { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE Data_QueryGetData() { return S_OK; }
static HRESULT STDMETHODCALLTYPE Data_GetCanonicalFormatEtc() { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE Data_SetData() { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE Data_EnumFormatEtc() { return E_NOTIMPL; }
static HRESULT STDMETHODCALLTYPE Data_DAdvise() { return OLE_E_ADVISENOTSUPPORTED; }
static HRESULT STDMETHODCALLTYPE Data_DUnadvise() { return OLE_E_ADVISENOTSUPPORTED; }
static HRESULT STDMETHODCALLTYPE Data_EnumDAdvise() { return OLE_E_ADVISENOTSUPPORTED; }

IDataObjectVtbl Data_Vtbl = {
    Data_QueryInterface,
    Data_AddRef,
    Data_Release,
    Data_GetData,
    Data_GetDataHere,
    Data_QueryGetData,
    Data_GetCanonicalFormatEtc,
    Data_SetData,
    Data_EnumFormatEtc,
    Data_DAdvise,
    Data_DUnadvise,
    Data_EnumDAdvise
};

struct drop_source
{
    IDropSource IDropSource_iface;
    LONG ref_count;
};

static HRESULT STDMETHODCALLTYPE Drop_QueryInterface(IDropSource* this, REFIID riid, void** ppv)
{
    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &IID_IDropSource)) {
        *ppv = this;
		this->lpVtbl->AddRef(this);
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE Drop_AddRef(IDropSource* this)
{
    struct drop_source* src = (struct drop_source*)this;
    return InterlockedIncrement(&src->ref_count);
}

static ULONG STDMETHODCALLTYPE Drop_Release(IDropSource* this)
{
    struct drop_source*src = (struct drop_source*)this;
    ULONG ref = InterlockedDecrement(&src->ref_count);
    if (!ref) free(src);
    return ref;
}

static HRESULT STDMETHODCALLTYPE Drop_QueryContinueDrag(
    IDropSource* this,
    BOOL escapePressed,
    DWORD keyState)
{
    if (escapePressed) return DRAGDROP_S_CANCEL;
    if (!(keyState & MK_LBUTTON)) return DRAGDROP_S_DROP;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE Drop_GiveFeedback(IDropSource* this, DWORD effect)
{
    return DRAGDROP_S_USEDEFAULTCURSORS;
}

IDropSourceVtbl Drop_Vtbl = {
    Drop_QueryInterface,
    Drop_AddRef,
    Drop_Release,
    Drop_QueryContinueDrag,
    Drop_GiveFeedback
};

void drag_drop_init(void)
{
    initialized = !FAILED(OleInitialize(NULL));
    if (!initialized)
		printf("failed to initialize drag drop\n");
}

void drag_drop_uninit(void)
{
    if (!initialized)
        return;

    OleUninitialize();
}

void drag_drop_start(char* file_path)
{
    if (!initialized)
        return;

	printf("drag dropping %s\n", file_path);

    struct file_object* obj = calloc(1, sizeof(struct file_object));
	obj->IDataObject_iface.lpVtbl = &Data_Vtbl;
	obj->ref_count = 1;
    obj->file_path = realloc_string(file_path);

    struct drop_source* src = calloc(1, sizeof(struct drop_source));
	src->IDropSource_iface.lpVtbl = &Drop_Vtbl;
	src->ref_count = 1;

	DWORD effect;
	HRESULT result = DoDragDrop(obj, src, DROPEFFECT_COPY, &effect);
	if (result == DRAGDROP_S_DROP)
		printf("success drag drop\n");
	else if (result == DRAGDROP_S_CANCEL)
		printf("cancelled drag drop\n");
	else
		printf("failed drag drop %d\n", result);

	obj->IDataObject_iface.lpVtbl->Release(obj);
	src->IDropSource_iface.lpVtbl->Release(src);

	return;

error:
    printf("failed to start drag drop\n");
}

#else

void drag_drop_init(void)
{
    printf("drag drop not implemented\n");
}

void drag_drop_uninit(void)
{
    printf("drag drop not implemented\n");
}

void drag_drop_start(char* file_path)
{
	(void)file_path;
    printf("drag drop not implemented\n");
}

#endif
