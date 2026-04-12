// Copyright (C) Explorer++ Project
// SPDX-License-Identifier: GPL-3.0-only
// See LICENSE in the top level directory

#include "stdafx.h"
#include "Explorer++.h"
#include "App.h"
#include "Config.h"
#include "FolderView.h"
#include "IDropFilesCallback.h"
#include "MainResource.h"
#include "MainToolbar.h"
#include "ResourceHelper.h"
#include "ServiceProvider.h"
#include "ShellBrowser/Columns.h"
#include "ShellBrowser/ShellBrowserImpl.h"
#include "ShellBrowser/ShellNavigationController.h"
#include "ShellBrowser/ViewModes.h"
#include "SortMenuBuilder.h"
#include "TabContainer.h"
#include "ThemeWindowTracker.h"
#include "ViewModeHelper.h"
#include "DuplicateFilenameHelper.h"
#include "FileOperations.h"
#include "../Helper/ClipboardHelper.h"
#include "../Helper/DataExchangeHelper.h"
#include "../Helper/DragDropHelper.h"
#include "../Helper/DropHandler.h"
#include "../Helper/Helper.h"
#include "../Helper/ListViewHelper.h"
#include "../Helper/MenuHelper.h"
#include "../Helper/ResourceHelper.h"
#include "../Helper/ShellHelper.h"
#include "../Helper/WindowHelper.h"
#include "../Helper/WinRTBaseWrapper.h"
#include <wil/com.h>
#include <filesystem>
#include <format>

namespace
{

template <typename T>
void AppendDialogTemplateValue(std::vector<BYTE> &buffer, const T &value)
{
	auto *data = reinterpret_cast<const BYTE *>(&value);
	buffer.insert(buffer.end(), data, data + sizeof(value));
}

void AppendDialogTemplateString(std::vector<BYTE> &buffer, const std::wstring &value)
{
	auto *data = reinterpret_cast<const BYTE *>(value.c_str());
	buffer.insert(buffer.end(), data, data + ((value.size() + 1) * sizeof(wchar_t)));
}

std::vector<BYTE> BuildDuplicateFileReplaceDialogTemplate(size_t matchCount)
{
	constexpr short DIALOG_WIDTH = 220;
	constexpr short SINGLE_DIALOG_HEIGHT = 72;
	constexpr short MULTI_DIALOG_BASE_HEIGHT = 102;

	short dialogHeight = SINGLE_DIALOG_HEIGHT;

	if (matchCount > 1)
	{
		int visibleRows = static_cast<int>(matchCount);

		if (visibleRows > 6)
		{
			visibleRows = 6;
		}

		dialogHeight = static_cast<short>(MULTI_DIALOG_BASE_HEIGHT + (visibleRows * 8));
	}

	DLGTEMPLATEEX dialogTemplate = {};
	dialogTemplate.dlgVer = 1;
	dialogTemplate.signature = 0xFFFF;
	dialogTemplate.helpID = 0;
	dialogTemplate.exStyle = WS_EX_CONTROLPARENT;
	dialogTemplate.style = DS_SHELLFONT | DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU;
	dialogTemplate.cDlgItems = 0;
	dialogTemplate.x = 0;
	dialogTemplate.y = 0;
	dialogTemplate.cx = DIALOG_WIDTH;
	dialogTemplate.cy = dialogHeight;

	std::vector<BYTE> buffer;
	buffer.reserve(sizeof(dialogTemplate) + 128);
	AppendDialogTemplateValue(buffer, dialogTemplate);

	WORD menu = 0;
	WORD windowClass = 0;
	WORD pointSize = 8;
	WORD fontWeight = 400;
	BYTE italic = 0;
	BYTE charset = 1;

	AppendDialogTemplateValue(buffer, menu);
	AppendDialogTemplateValue(buffer, windowClass);
	AppendDialogTemplateString(buffer, App::APP_NAME);
	AppendDialogTemplateValue(buffer, pointSize);
	AppendDialogTemplateValue(buffer, fontWeight);
	AppendDialogTemplateValue(buffer, italic);
	AppendDialogTemplateValue(buffer, charset);
	AppendDialogTemplateString(buffer, L"MS Shell Dlg");

	return buffer;
}

std::wstring ConvertDialogLineEndings(const std::wstring &text)
{
	std::wstring converted;
	converted.reserve(text.size() + 16);

	for (size_t i = 0; i < text.size(); i++)
	{
		if (text[i] == L'\n' && (i == 0 || text[i - 1] != L'\r'))
		{
			converted += L"\r\n";
		}
		else
		{
			converted.push_back(text[i]);
		}
	}

	return converted;
}

class DuplicateFileReplaceDialog
{
public:
	enum class Result
	{
		Replace,
		KeepBoth,
		Cancel
	};

	static Result Show(HWND parent, App *app, std::wstring instruction, std::wstring content,
		size_t matchCount)
	{
		DuplicateFileReplaceDialog dialog(parent, app, std::move(instruction), std::move(content),
			matchCount);
		return dialog.ShowModal();
	}

private:
	DuplicateFileReplaceDialog(HWND parent, App *app, std::wstring instruction,
		std::wstring content, size_t matchCount) :
		m_parent(parent),
		m_app(app),
		m_instruction(std::move(instruction)),
		m_content(std::move(content)),
		m_matchCount(matchCount)
	{
	}

	Result ShowModal()
	{
		m_dialogTemplate = BuildDuplicateFileReplaceDialogTemplate(m_matchCount);

		INT_PTR result = DialogBoxIndirectParam(GetModuleHandle(nullptr),
			reinterpret_cast<DLGTEMPLATE *>(m_dialogTemplate.data()), m_parent, DialogProcStub,
			reinterpret_cast<LPARAM>(this));

		if (result == -1)
		{
			LOG_SYSRESULT(GetLastError());
			return Result::KeepBoth;
		}

		switch (result)
		{
		case IDYES:
			return Result::Replace;

		case IDNO:
			return Result::KeepBoth;

		default:
			return Result::Cancel;
		}
	}

	static INT_PTR CALLBACK DialogProcStub(HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		auto *duplicateDialog =
			reinterpret_cast<DuplicateFileReplaceDialog *>(GetWindowLongPtr(dialog, GWLP_USERDATA));

		if (msg == WM_INITDIALOG)
		{
			duplicateDialog = reinterpret_cast<DuplicateFileReplaceDialog *>(lParam);
			SetWindowLongPtr(dialog, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(duplicateDialog));
		}

		if (!duplicateDialog)
		{
			return FALSE;
		}

		return duplicateDialog->DialogProc(dialog, msg, wParam, lParam);
	}

	INT_PTR DialogProc(HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		UNREFERENCED_PARAMETER(lParam);

		switch (msg)
		{
		case WM_INITDIALOG:
			return OnInitDialog(dialog);

		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
			case IDYES:
				EndDialog(dialog, IDYES);
				return TRUE;

			case IDNO:
				EndDialog(dialog, IDNO);
				return TRUE;

			case IDCANCEL:
				EndDialog(dialog, IDCANCEL);
				return TRUE;
			}
			break;

		case WM_CLOSE:
			EndDialog(dialog, IDCANCEL);
			return TRUE;

		case WM_NCDESTROY:
			m_themeWindowTracker.reset();
			SetWindowLongPtr(dialog, GWLP_USERDATA, 0);
			break;
		}

		return FALSE;
	}

	INT_PTR OnInitDialog(HWND dialog)
	{
		m_hDlg = dialog;

		constexpr int MARGIN = 14;
		constexpr int ICON_SIZE = 32;
		constexpr int ICON_TEXT_SPACING = 10;
		constexpr int BUTTON_WIDTH = 76;
		constexpr int BUTTON_HEIGHT = 23;
		constexpr int BUTTON_SPACING = 8;
		constexpr int CONTENT_SPACING = 10;

		RECT clientRect;
		GetClientRect(m_hDlg, &clientRect);

		int textLeft = MARGIN + ICON_SIZE + ICON_TEXT_SPACING;
		int textWidth = clientRect.right - textLeft - MARGIN;
		int buttonTop = clientRect.bottom - MARGIN - BUTTON_HEIGHT;
		int cancelLeft = clientRect.right - MARGIN - BUTTON_WIDTH;
		int noLeft = cancelLeft - BUTTON_SPACING - BUTTON_WIDTH;
		int yesLeft = noLeft - BUTTON_SPACING - BUTTON_WIDTH;
		int instructionTop = MARGIN;
		int instructionHeight = 20;
		int contentTop = instructionTop + instructionHeight + CONTENT_SPACING;
		int contentHeight = buttonTop - CONTENT_SPACING - contentTop;

		if (contentHeight < 30)
		{
			contentHeight = 30;
		}

		auto createControl = [this](DWORD exStyle, const wchar_t *className, const wchar_t *text,
									DWORD style, int x, int y, int width, int height,
									int controlId) -> HWND
		{
			HWND control = CreateWindowEx(exStyle, className, text, style, x, y, width, height,
				m_hDlg, reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
				GetModuleHandle(nullptr), nullptr);
			CHECK(control);
			return control;
		};

		auto iconControl = createControl(0, WC_STATIC, L"", WS_CHILD | WS_VISIBLE | SS_ICON, MARGIN,
			instructionTop + 2, ICON_SIZE, ICON_SIZE, 0);
		SendMessage(iconControl, STM_SETICON, reinterpret_cast<WPARAM>(LoadIcon(nullptr,
			IDI_INFORMATION)), 0);

		auto instructionControl = createControl(0, WC_STATIC, m_instruction.c_str(),
			WS_CHILD | WS_VISIBLE, textLeft, instructionTop, textWidth, instructionHeight, 0);

		HFONT dialogFont = reinterpret_cast<HFONT>(SendMessage(m_hDlg, WM_GETFONT, 0, 0));

		if (!dialogFont)
		{
			dialogFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
		}

		LOGFONT logFont;

		if (GetObject(dialogFont, sizeof(logFont), &logFont) != 0)
		{
			logFont.lfWeight = FW_SEMIBOLD;
			m_instructionFont.reset(CreateFontIndirect(&logFont));
		}

		SendMessage(instructionControl, WM_SETFONT,
			reinterpret_cast<WPARAM>(m_instructionFont.get() ? m_instructionFont.get() : dialogFont),
			TRUE);

		if (m_matchCount > 1)
		{
			DWORD contentStyle =
				WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL;
			auto contentControl =
				createControl(WS_EX_CLIENTEDGE, WC_EDIT, ConvertDialogLineEndings(m_content).c_str(),
					contentStyle, textLeft, contentTop, textWidth, contentHeight, 0);
			SendMessage(contentControl, WM_SETFONT, reinterpret_cast<WPARAM>(dialogFont), TRUE);
		}
		else
		{
			auto contentControl = createControl(0, WC_STATIC, m_content.c_str(), WS_CHILD | WS_VISIBLE,
				textLeft, contentTop, textWidth, contentHeight, 0);
			SendMessage(contentControl, WM_SETFONT, reinterpret_cast<WPARAM>(dialogFont), TRUE);
		}

		auto yesButton = createControl(0, WC_BUTTON, L"&Yes",
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, yesLeft, buttonTop, BUTTON_WIDTH,
			BUTTON_HEIGHT, IDYES);
		SendMessage(yesButton, WM_SETFONT, reinterpret_cast<WPARAM>(dialogFont), TRUE);

		m_noButton = createControl(0, WC_BUTTON, L"&No",
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, noLeft, buttonTop, BUTTON_WIDTH,
			BUTTON_HEIGHT, IDNO);
		SendMessage(m_noButton, WM_SETFONT, reinterpret_cast<WPARAM>(dialogFont), TRUE);

		auto cancelButton = createControl(0, WC_BUTTON, L"Cancel",
			WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, cancelLeft, buttonTop, BUTTON_WIDTH,
			BUTTON_HEIGHT, IDCANCEL);
		SendMessage(cancelButton, WM_SETFONT, reinterpret_cast<WPARAM>(dialogFont), TRUE);

		SendMessage(m_hDlg, DM_SETDEFID, IDNO, 0);

		m_themeWindowTracker = std::make_unique<ThemeWindowTracker>(m_hDlg,
			m_app->GetThemeManager());

		CenterWindow(m_parent, m_hDlg);
		SetFocus(m_noButton);
		return FALSE;
	}

	const HWND m_parent;
	App *const m_app;
	HWND m_hDlg = nullptr;
	HWND m_noButton = nullptr;
	std::wstring m_instruction;
	std::wstring m_content;
	size_t m_matchCount;
	std::vector<BYTE> m_dialogTemplate;
	std::unique_ptr<ThemeWindowTracker> m_themeWindowTracker;
	wil::unique_hfont m_instructionFont;
};

}

LRESULT Explorerplusplus::OnListViewKeyDown(LPARAM lParam)
{
	LV_KEYDOWN *keyDown = reinterpret_cast<LV_KEYDOWN *>(lParam);

	switch (keyDown->wVKey)
	{
	case 'V':
		if (IsKeyDown(VK_CONTROL) && !IsKeyDown(VK_SHIFT) && !IsKeyDown(VK_MENU))
		{
			OnListViewPaste();
		}
		break;

	case VK_INSERT:
		if (!IsKeyDown(VK_CONTROL) && IsKeyDown(VK_SHIFT) && !IsKeyDown(VK_MENU))
		{
			OnListViewPaste();
		}
		break;
	}

	return 0;
}

bool Explorerplusplus::TryHandleDuplicatePaste(IDataObject *dataObject)
{
	FORMATETC ftc = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	wil::unique_stg_medium stg;
	HRESULT hr = dataObject->GetData(&ftc, &stg);

	if (FAILED(hr))
	{
		return false;
	}

	auto files = ReadHDropDataFromGlobal(stg.hGlobal);

	if (!files || files->empty())
	{
		return false;
	}

	const auto &selectedTab = GetActivePane()->GetTabContainer()->GetSelectedTab();
	auto destDir = selectedTab.GetShellBrowserImpl()->GetDirectoryPath();
	auto matches = FindDuplicateMatches(*files, destDir);

	if (matches.empty())
	{
		return false;
	}

	std::wstring instruction;
	std::wstring content;

	if (matches.size() == 1)
	{
		auto sourceFilename =
			std::filesystem::path(matches[0].sourcePath).filename().wstring();
		instruction = L"Replace existing file?";
		content = std::format(L"\"{}\" matches existing file \"{}\".",
			sourceFilename, matches[0].targetName);
	}
	else
	{
		instruction = L"Replace existing files?";
		content = L"The following files match existing files:\n\n";

		for (const auto &match : matches)
		{
			auto sourceFilename = std::filesystem::path(match.sourcePath).filename().wstring();
			content += std::format(L"  {} \u2192 {}\n", sourceFilename, match.targetName);
		}
	}

	auto result = DuplicateFileReplaceDialog::Show(m_hContainer, m_app, std::move(instruction),
		std::move(content), matches.size());

	if (result == DuplicateFileReplaceDialog::Result::Cancel)
	{
		// Cancel: abort the entire paste operation.
		return true;
	}

	if (result != DuplicateFileReplaceDialog::Result::Replace)
	{
		// No: fall through to normal shell paste.
		return false;
	}

	DWORD preferredEffect = DROPEFFECT_COPY;
	GetPreferredDropEffect(dataObject, preferredEffect);
	bool isMove = (preferredEffect & DROPEFFECT_MOVE) != 0;

	hr = PerformDuplicateReplace(m_hContainer, *files, matches, destDir, isMove);

	return SUCCEEDED(hr);
}

void Explorerplusplus::OnListViewPaste()
{
	auto clipboardObject = m_app->GetPlatformContext()->GetClipboardStore()->GetDataObject();

	if (!clipboardObject)
	{
		return;
	}

	if (TryHandleDuplicatePaste(clipboardObject.get()))
	{
		return;
	}

	const auto &selectedTab = GetActivePane()->GetTabContainer()->GetSelectedTab();
	auto directory = selectedTab.GetShellBrowserImpl()->GetDirectoryIdl();

	if (CanShellPasteDataObject(directory.get(), clipboardObject.get(), PasteType::Normal))
	{
		auto directPasteResult =
			::FileOperations::PasteDataObject(m_hContainer, directory.get(), clipboardObject.get());

		if (directPasteResult.has_value())
		{
			return;
		}

		auto serviceProvider = winrt::make_self<ServiceProvider>();
		serviceProvider->RegisterService(IID_IFolderView,
			winrt::make<FolderView>(selectedTab.GetShellBrowserImpl()->GetWeakPtr()));

		ExecuteActionFromContextMenu(directory.get(), {},
			selectedTab.GetShellBrowserImpl()->GetListView(), L"paste", 0, serviceProvider.get());
	}
	else
	{
		TCHAR szDestination[MAX_PATH + 1];

		/* DO NOT use the internal current directory string.
		 Files are copied asynchronously, so a change of directory
		 will cause the destination directory to change in the
		 middle of the copy operation. */
		StringCchCopy(szDestination, std::size(szDestination),
			selectedTab.GetShellBrowserImpl()->GetDirectoryPath().c_str());

		/* Also, the string must be double NULL terminated. */
		szDestination[lstrlen(szDestination) + 1] = '\0';

		DropHandler *pDropHandler = DropHandler::CreateNew();
		DropFilesCallback dropFilesCallback{ this };
		pDropHandler->CopyClipboardData(clipboardObject.get(), m_hContainer, szDestination,
			&dropFilesCallback);
		pDropHandler->Release();
	}
}

int Explorerplusplus::HighlightSimilarFiles(HWND ListView) const
{
	BOOL bSimilarTypes;
	int iSelected;
	int nItems;
	int nSimilar = 0;
	int i = 0;

	iSelected = ListView_GetNextItem(ListView, -1, LVNI_SELECTED);

	if (iSelected == -1)
		return -1;

	std::wstring testFile = m_pActiveShellBrowser->GetItemFullName(iSelected);

	nItems = ListView_GetItemCount(ListView);

	for (i = 0; i < nItems; i++)
	{
		std::wstring fullFileName = m_pActiveShellBrowser->GetItemFullName(i);

		bSimilarTypes = CompareFileTypes(fullFileName.c_str(), testFile.c_str());

		if (bSimilarTypes)
		{
			ListViewHelper::SelectItem(ListView, i, true);
			nSimilar++;
		}
		else
		{
			ListViewHelper::SelectItem(ListView, i, false);
		}
	}

	return nSimilar;
}
