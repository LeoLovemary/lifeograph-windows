/***********************************************************************************

    Copyright (C) 2014-2017 Ahmet Ozturk (aoz_2@yahoo.com)

    This file is part of Lifeograph.

    Lifeograph is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Lifeograph is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Lifeograph.  If not, see <http://www.gnu.org/licenses/>.

***********************************************************************************/


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <winsock2.h> // just to silence the compiler
#include <windows.h>
#include <windowsx.h> // GET_X_LPARAM
#include <richedit.h>
#include <string>
#include <cstdlib>
#include <cassert>

#ifndef RICHEDIT_CLASS
    #ifdef UNICODE
        #define RICHEDIT_CLASS "RichEdit20W"
    #else
        #define RICHEDIT_CLASS "RichEdit20A"
    #endif
#endif

#include "../rc/resource.h"
#include "strings.hpp"
#include "lifeograph.hpp"
#include "win_app_window.hpp"
#include "win_dialog_password.hpp"
#include "win_dialog_tags.hpp"


using namespace LIFEO;


// NON-MEMBER PROCEDURES
inline static LRESULT CALLBACK
app_window_proc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    return WinAppWindow::p->proc( hwnd, msg, wParam, lParam );
}

inline BOOL CALLBACK
toolbar_proc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    return WinAppWindow::p->proc_toolbar( hwnd, msg, wParam, lParam );
}

int CALLBACK
list_compare_func( LPARAM lp1, LPARAM lp2, LPARAM lParamSort )
{
    // SORT BY DATE (ONLY DESCENDINGLY FOR NOW)
    DiaryElement* item1 = Diary::d->get_element( lp1 );
    DiaryElement* item2 = Diary::d->get_element( lp2 );
    if( !( item1 && item2 ) )
        return 0;
    else
    if( item1->get_type() == DiaryElement::ET_DIARY )
    return -1;

    int direction( ( item1->get_date().is_ordinal() && item2->get_date().is_ordinal() ) ? -1 : 1 );

    if( item1->get_date() > item2->get_date() )
        return -direction;
    else
    if( item1->get_date() < item2->get_date() )
        return direction;
    else
        return 0;
}

LRESULT CALLBACK
richedit_proc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam,
               UINT_PTR uIdSubclass, DWORD_PTR dwRefData )
{
    switch( msg )
    {
        case WM_KEYDOWN:
        {
            switch( wparam )
            {
                case VK_RETURN:
                    if( WinAppWindow::p->m_entry_view->m_richedit->handle_new_line() )
                        return TRUE;
                    break;
                case VK_SPACE:
                    if( WinAppWindow::p->m_entry_view->m_richedit->handle_space() )
                        return TRUE;
                    break;
            }
            break;
        }
        case WM_RBUTTONDOWN:
        {
            //m_entry_view->m_richedit->handle_right_click();
            DWORD dwpos = GetMessagePos();
            WinAppWindow::p->display_context_menu( WinAppWindow::p->m_hwnd,
                                                   GET_X_LPARAM( dwpos ),
                                                   GET_Y_LPARAM( dwpos ),
                                                   IDM_TEXT_EDITOR );
            break;
        }
    }

    return DefSubclassProc( hwnd, msg, wparam, lparam );
}

LRESULT CALLBACK
calendar_proc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam,
               UINT_PTR uIdSubclass, DWORD_PTR dwRefData )
{
    switch( msg )
    {
        case WM_LBUTTONDBLCLK:
        {
            POINT pt;
            pt.x = LOWORD( lparam );
            pt.y = HIWORD( lparam );
            WinAppWindow::p->handle_calendar_doubleclick( pt );
            return TRUE;
        }
        // TODO: add a menu for chapters..
        //case WM_RBUTTONUP:
            //return TRUE;
    }
    
    return DefSubclassProc( hwnd, msg, wparam, lparam );
}

WinAppWindow* WinAppWindow::p = NULL;

// CONSTRUCTOR
WinAppWindow::WinAppWindow()
:    m_entry_view( NULL ), m_seconds_remaining( LOGOUT_COUNTDOWN + 1 ), m_auto_logout_status( 1 )
{
    p = this;

    Cipher::init();

    Diary::d = new Diary;
    m_login = new WinLogin;
    m_entry_view = new EntryView;
    m_diary_view = new DiaryView;
}

WinAppWindow::~WinAppWindow()
{
    if( Diary::d )
        delete Diary::d;
}

int
WinAppWindow::run()
{
    WNDCLASSEX wc;
    HWND hwnd;
    MSG msg;

    wc.cbSize        = sizeof( WNDCLASSEX );
    wc.style         = CS_DBLCLKS;
    wc.lpfnWndProc   = app_window_proc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = Lifeograph::hInst;
    wc.hIcon         = LoadIcon( Lifeograph::hInst, L"A" );
    wc.hCursor       = LoadCursor( NULL, IDC_ARROW );
    wc.hbrBackground = ( HBRUSH ) ( COLOR_WINDOW + 1 );
    wc.lpszMenuName  = L"";//MAKEINTRESOURCE( IDM_MAIN );
    wc.lpszClassName = L"WindowClass";
    wc.hIconSm       = LoadIcon( Lifeograph::hInst, L"A" ); // A is name used by project icons
    
    if( !RegisterClassEx( &wc ) )
    {
        MessageBoxA( 0, "Window Registration Failed!", "Error!",
                     MB_ICONEXCLAMATION|MB_OK|MB_SYSTEMMODAL );
        return 0;
    }

    hwnd = CreateWindowEx( WS_EX_ACCEPTFILES|WS_EX_WINDOWEDGE,
                           L"WindowClass",
                           L"Lifeograph",
                           WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           640, 480,
                           NULL, NULL, Lifeograph::hInst, NULL );

    if( hwnd == NULL )
    {
        MessageBoxA( 0, "Window Creation Failed!", "Error!",
                     MB_ICONEXCLAMATION|MB_OK|MB_SYSTEMMODAL );
        return 0;
    }

    m_haccel = LoadAccelerators( Lifeograph::hInst, L"LIFEOACCELERATORS" );

    ShowWindow( hwnd, 1 );
    UpdateWindow( hwnd );

    while( GetMessage( &msg, NULL, 0, 0 ) > 0 )
    {
        if( !TranslateAccelerator( hwnd, m_haccel, &msg ) )
        {
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
    }
    return msg.wParam;
}

LRESULT
WinAppWindow::proc( HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam )
{
    switch( Message )
    {
        case WM_CREATE:
            m_hwnd = hwnd;
            handle_create();
            break;
        case WM_SIZE:
            if( wParam != SIZE_MINIMIZED )
                handle_resize( LOWORD( lParam ), HIWORD( lParam ) );
            break;
        case WM_SETFOCUS:
            SetFocus( GetDlgItem( hwnd, IDRT_MAIN ) );
            break;
        case WM_ENTERMENULOOP:
            if( wParam == true )
                update_menu();
            break;
        case WM_COMMAND:
            switch( LOWORD( wParam ) )
            {
                case IDMI_DIARY_CREATE:
                    if( Lifeograph::loginstatus == Lifeograph::LOGGED_IN )
                        if( ! finish_editing() )
                            break;
                    m_login->create_new_diary();
                    break;
                case IDMI_DIARY_OPEN:
                    if( Lifeograph::loginstatus == Lifeograph::LOGGED_IN )
                        if( ! finish_editing() )
                            break;
                    m_login->add_existing_diary();
                    break;
                case IDMI_DIARY_CHANGE_PASSWORD:
                    if( !authorize() )
                        break;
                case IDMI_DIARY_ENCRYPT:
                    DialogPassword::launch( m_hwnd, Diary::d, DialogPassword::PD_NEW );
                    break;
                case IDMI_DIARY_EXPORT:
                    if( Lifeograph::loginstatus == Lifeograph::LOGGED_IN )
                        m_diary_view->export_diary();
                    break;
                case IDMI_QUIT_WO_SAVE:
                    PostMessage( hwnd, WM_CLOSE, 0, true );
                    break;
                case IDMI_QUIT:
                    PostMessage( hwnd, WM_CLOSE, 0, 0 );
                    break;
                case IDMI_DIARY_ADD_NCHAPTER:
                    add_chapter( Diary::d->get_topics(), NULL );
                    break;
                case IDMI_DIARY_ADD_FCHAPTER:
                    add_chapter( Diary::d->get_groups(), NULL );
                    break;
                case IDMI_DIARY_ADD_DCHAPTER:
                {
                    Date date( m_date_selected );
                    add_chapter( Diary::d->get_current_chapter_ctg(), &date );
                    break;
                }
                case IDMI_ENTRY_TODO_NOT:
                    m_entry_view->set_todo_status( ES::NOT_TODO );
                    break;
                case IDMI_ENTRY_TODO_OPEN:
                    m_entry_view->set_todo_status( ES::TODO );
                    break;
                case IDMI_ENTRY_TODO_PROGRESSED:
                    m_entry_view->set_todo_status( ES::PROGRESSED );
                    break;
                case IDMI_ENTRY_TODO_DONE:
                    m_entry_view->set_todo_status( ES::DONE );
                    break;
                case IDMI_ENTRY_TODO_CANCELED:
                    m_entry_view->set_todo_status( ES::CANCELED );
                    break;
                case IDMI_ENTRY_FAVORITE:
                    m_entry_view->toggle_favoredness();
                    break;
                case IDMI_ENTRY_DISMISS:
                    m_entry_view->dismiss_entry();
                    break;
                case IDMI_CHAPTER_ADD_ENTRY:
                {
                    DiaryElement* elem = get_selected_list_elem();
                    if( elem && ( elem->get_type() == DiaryElement::ET_TOPIC ||
                                  elem->get_type() == DiaryElement::ET_GROUP ) )
                    {
                        Entry* entry = Diary::d->create_entry(
                                dynamic_cast< Chapter* >( elem )->get_free_order().m_date );

                        update_entry_list();
                        update_calendar();

                        entry->show();

                        SetFocus( m_entry_view->m_richedit->m_hwnd );
                    }
                    break;
                }
                case IDMI_CHAPTER_RENAME:
                {
                    HTREEITEM item = TreeView_GetSelection( m_list );
                    if( item )
                        TreeView_EditLabel( m_list, item );
                    break;
                }
                case IDMI_CHAPTER_DISMISS:
                {
                    DiaryElement* elem = get_selected_list_elem();
                    if( elem && ( elem->get_type() == DiaryElement::ET_CHAPTER ||
                                  elem->get_type() == DiaryElement::ET_TOPIC ||
                                  elem->get_type() == DiaryElement::ET_GROUP ) )
                    {
                        if( confirm_dismiss_element( elem ) )
                        {
                            Diary::d->dismiss_chapter( dynamic_cast< Chapter* >( elem ), false );
                            update_entry_list();
                        }
                    }
                    break;
                }
                case IDMI_FILTER_TODO_NOT:
                {
                    ElemStatus fs( Diary::d->get_filter_status() );
                    Diary::d->get_filter()->set_todo( !( fs & ES::SHOW_NOT_TODO ),
                                                      fs & ES::SHOW_TODO,
                                                      fs & ES::SHOW_PROGRESSED,
                                                      fs & ES::SHOW_DONE,
                                                      fs & ES::SHOW_CANCELED );
                    update_entry_list();
                    update_calendar();
                    break;
                }
                case IDMI_FILTER_TODO_OPEN:
                {
                    ElemStatus fs( Diary::d->get_filter_status() );
                    Diary::d->get_filter()->set_todo( fs & ES::SHOW_NOT_TODO,
                                                      !( fs & ES::SHOW_TODO ),
                                                      fs & ES::SHOW_PROGRESSED,
                                                      fs & ES::SHOW_DONE,
                                                      fs & ES::SHOW_CANCELED );
                    update_entry_list();
                    update_calendar();
                    break;
                }
                case IDMI_FILTER_TODO_PROGRESSED:
                {
                    ElemStatus fs( Diary::d->get_filter_status() );
                    Diary::d->get_filter()->set_todo( fs & ES::SHOW_NOT_TODO,
                                                      fs & ES::SHOW_TODO,
                                                      !( fs & ES::SHOW_PROGRESSED ),
                                                      fs & ES::SHOW_DONE,
                                                      fs & ES::SHOW_CANCELED );
                    update_entry_list();
                    update_calendar();
                    break;
                }
                case IDMI_FILTER_TODO_DONE:
                {
                    ElemStatus fs( Diary::d->get_filter_status() );
                    Diary::d->get_filter()->set_todo( fs & ES::SHOW_NOT_TODO,
                                                      fs & ES::SHOW_TODO,
                                                      fs & ES::SHOW_PROGRESSED,
                                                      !( fs & ES::SHOW_DONE ),
                                                      fs & ES::SHOW_CANCELED );
                    update_entry_list();
                    update_calendar();
                    break;
                }
                case IDMI_FILTER_TODO_CANCELED:
                {
                    ElemStatus fs( Diary::d->get_filter_status() );
                    Diary::d->get_filter()->set_todo( fs & ES::SHOW_NOT_TODO,
                                                      fs & ES::SHOW_TODO,
                                                      fs & ES::SHOW_PROGRESSED,
                                                      fs & ES::SHOW_DONE,
                                                      !( fs & ES::SHOW_CANCELED ) );
                    update_entry_list();
                    update_calendar();
                    break;
                }
                case IDMI_FILTER_FAVORITES:
                    if( Diary::d->get_filter_status() & ES::SHOW_NOT_FAVORED )
                        Diary::d->get_filter()->set_favorites( true, false );
                    else
                        Diary::d->get_filter()->set_favorites( true, true );
                    update_entry_list();
                    update_calendar();
                    break;
                case IDMI_FILTER_TAGS:
                    if( DialogTags::launch( m_hwnd, Diary::d ) == SUCCESS )
                    {
                        update_entry_list();
                        update_calendar();
                    }
                    break;
                case IDMI_FILTER_RESET:
                    Diary::d->get_filter()->set( Diary::d->get_filter_default() );
                    Diary::d->get_filter()->set_status_outstanding();
                    update_entry_list();
                    update_calendar();
                    break;
                case IDMI_FILTER_SAVE:
                    Diary::d->get_filter_default()->set( Diary::d->get_filter() );
                    break;
                case IDRT_MAIN:
                    if( HIWORD( wParam ) == EN_CHANGE )
                        m_entry_view->m_richedit->handle_change();
                    break;
                case IDMI_TEXT_NOT_LIST:
                    m_entry_view->m_richedit->set_list_item_mark( 0 );
                    break;
                case IDMI_TEXT_TODO_OPEN:
                    m_entry_view->m_richedit->set_list_item_mark( ' ' );
                    break;
                case IDMI_TEXT_TODO_PROGRESSED:
                    m_entry_view->m_richedit->set_list_item_mark( '~' );
                    break;
                case IDMI_TEXT_TODO_DONE:
                    m_entry_view->m_richedit->set_list_item_mark( '+' );
                    break;
                case IDMI_TEXT_TODO_CANCELED:
                    m_entry_view->m_richedit->set_list_item_mark( 'x' );
                    break;
                case IDMI_TEXT_BULLET:
                    m_entry_view->m_richedit->set_list_item_mark( '*' );
                    break;
                case IDA_C_D:
                    add_todo_entry();
                    break;
                case IDA_C_E:
                    show_today();
                    break;
                case IDA_C_T:
                    if( Lifeograph::loginstatus == Lifeograph::LOGGED_IN &&
                        m_entry_view->get_element() != nullptr )
                        start_tag_dialog( "" );
                    break;
                case IDA_A_UP:
                    if( Lifeograph::loginstatus == Lifeograph::LOGGED_IN &&
                        m_entry_view->get_element() != nullptr )
                        m_entry_view->m_richedit->move_line_up();
                    break;
                case IDA_A_DOWN:
                    if( Lifeograph::loginstatus == Lifeograph::LOGGED_IN &&
                        m_entry_view->get_element() != nullptr )
                        m_entry_view->m_richedit->move_line_down();
                    break;
            }
            break;
        case WM_NOTIFY:
            return handle_notify( ( int ) wParam, lParam );
        case WM_MOUSEMOVE:
            if( !wParam )
                m_entry_view->m_tag_widget->handle_mouse_move( LOWORD( lParam ), HIWORD( lParam ) );
            break;
        case WM_LBUTTONDBLCLK:
            if( wParam == MK_LBUTTON )
                m_entry_view->m_tag_widget->handle_click( LOWORD( lParam ), HIWORD( lParam ) );
            break;
        case WM_DROPFILES:
        {
            if( Lifeograph::loginstatus == Lifeograph::LOGGED_IN )
                if( ! finish_editing() )
                    break;
                    
            char szFileName[ MAX_PATH ];
            DragQueryFileA( ( HDROP ) wParam, 0, szFileName, MAX_PATH );
            DragFinish( ( HDROP ) wParam );

            m_login->m_path_cur = szFileName;
            if( m_login->open_selected_diary( false ) != LIFEO::SUCCESS )
                MessageBoxA( m_hwnd, "Load of file failed.", "", MB_OK|MB_ICONERROR );

            break;
        }
        case WM_TIMER:
            if( wParam == m_backup_timer_id )
                write_backup();
            break;
        case WM_CLOSE:
            if( Lifeograph::p->loginstatus == Lifeograph::LOGGED_IN )
                if( ! finish_editing( ! lParam ) )
                    break;
            DestroyWindow( hwnd );
            break;
        case WM_DESTROY:
            PostQuitMessage( 0 );
            break;
        case WM_PAINT:
            if( GetUpdateRect( m_hwnd, NULL, false ) )
                m_entry_view->m_tag_widget->handle_draw();
            // BEWARE: no break, WM_PAINT must be processed by the system or bad things happen
        default:
            return DefWindowProc( hwnd, Message, wParam, lParam );
    }
    return 0;
}

BOOL
WinAppWindow::proc_toolbar( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
    switch( msg )
    {
        case WM_INITDIALOG:
        {
            m_edit_search = GetDlgItem( hwnd, IDE_SEARCH );
            m_edit_date = GetDlgItem( hwnd, IDE_ELEM_DATE );
            m_button_title = GetDlgItem( hwnd, IDB_ELEM_TITLE );
            m_button_today = GetDlgItem( hwnd, IDB_TODAY );
            m_button_elem = GetDlgItem( hwnd, IDB_ELEM );
            m_button_filter = GetDlgItem( hwnd, IDB_FILTER );
            m_button_about = GetDlgItem( hwnd, IDB_ABOUT );
            SetWindowSubclass( m_edit_date, WAO_advanced_edit_proc, 0, 0 );
            
            WAO_set_icon( m_button_filter,
                          LoadIcon( GetModuleHandle( NULL ), MAKEINTRESOURCE( IDI_FILTER16 ) ) );

            ShowWindow( m_button_today, SW_HIDE );
            ShowWindow( m_button_elem, SW_HIDE );
            ShowWindow( m_button_title, SW_HIDE );
            ShowWindow( m_button_filter, SW_HIDE );
            ShowWindow( m_edit_date, SW_HIDE );
            ShowWindow( m_edit_search, SW_HIDE );

            return TRUE;
        }
        case WAOM_EDITACTIVATED:
        {
            wchar_t cstr[ 128 ];
            GetWindowText( m_edit_date, cstr, 128 );
            std::string str = convert_utf16_to_8( cstr );
            Date date( str );
            if( !date.is_set() )
                return TRUE;

            if( !date.is_ordinal() )
                date.reset_order_1();

            Diary::d->set_entry_date( m_entry_view->get_element(), date );

            update_entry_list();
            SetWindowText( m_button_title,
                           HELPERS::convert_utf8_to_16( m_entry_view->get_title_str() ) );
            //AppWindow::p->panel_diary->select_date_in_calendar( m_ptr2elem->get_date() );
        }
        case WAOM_EDITABORTED:
            SetFocus( m_entry_view->m_richedit->m_hwnd );
            return TRUE;
        case WM_COMMAND:
            switch( LOWORD( wParam ) )
            {
                case IDB_DIARY:
                {
                    RECT rect;
                    GetWindowRect( GetDlgItem( hwnd, IDB_DIARY ), &rect );
                    display_context_menu( m_hwnd, rect.left, rect.bottom, IDM_DIARY );
                    return TRUE;
                }
                case IDB_ABOUT:
                    show_about();
                    return TRUE;
                case IDB_TODAY:
                    show_today();
                    return TRUE;
                case IDB_ELEM:
                {
                    RECT rect;
                    GetWindowRect( m_button_elem, &rect );
                    display_context_menu( m_hwnd, rect.left, rect.bottom, IDM_ENTRY );
                    return TRUE;
                }
                case IDB_FILTER:
                {
                    RECT rect;
                    GetWindowRect( m_button_filter, &rect );
                    display_context_menu( m_hwnd, rect.left, rect.bottom, IDM_FILTER );
                    return TRUE;
                }
                case IDE_ELEM_DATE:
                    if( HIWORD( wParam ) == EN_KILLFOCUS )
                    {
                        ShowWindow( m_edit_date, SW_HIDE );
                        ShowWindow( m_button_title, SW_SHOW );
                    }
                    return TRUE;
                case IDB_ELEM_TITLE:
                    ShowWindow( m_button_title, SW_HIDE );
                    ShowWindow( m_edit_date, SW_SHOW );
                    SetWindowText( m_edit_date,
                            HELPERS::convert_utf8_to_16(
                                    m_entry_view->get_element()->get_date().format_string() ) );
                    SetFocus( m_edit_date );
                    return TRUE;
                case IDE_SEARCH:
                    if( HIWORD( wParam ) == EN_CHANGE )
                        handle_search_string_changed();
                    return TRUE;
            }
            return FALSE;
        default:
            return WAO_toolbar_proc( hwnd, msg, wParam, lParam );
    }
}

void
WinAppWindow::handle_create()
{
    INITCOMMONCONTROLSEX iccx;
    iccx.dwSize = sizeof( INITCOMMONCONTROLSEX );
    iccx.dwICC  = ICC_LISTVIEW_CLASSES | ICC_DATE_CLASSES;
    InitCommonControlsEx( &iccx );

    // try to load latest version of rich edit control
    static HINSTANCE hlib = LoadLibrary( L"RICHED20.DLL" );
    if( !hlib )
    {
        MessageBoxA( NULL, "Failed to load Rich Edit", "Error", MB_OK | MB_ICONERROR );
        return;
    }
    
    // TOOLBAR
    m_toolbar = CreateDialog( Lifeograph::hInst, MAKEINTRESOURCE( IDTB_MAIN ),
                              m_hwnd, toolbar_proc );

    // RICH EDIT
    m_entry_view->m_richedit->m_hwnd =
            CreateWindowEx( 0, //WS_EX_CLIENTEDGE,
                            RICHEDIT_CLASS, L"",
                    	    WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_WANTRETURN,
                            0, 0, CW_USEDEFAULT, CW_USEDEFAULT,
                            m_hwnd, ( HMENU ) IDRT_MAIN, GetModuleHandle( NULL ), NULL );

    SendMessage( m_entry_view->m_richedit->m_hwnd, WM_SETFONT,
                 ( WPARAM ) GetStockObject( DEFAULT_GUI_FONT ), MAKELPARAM( TRUE, 0 ) );
    SendMessage( m_entry_view->m_richedit->m_hwnd, EM_SETEVENTMASK, 0,
                 ( LPARAM ) ENM_CHANGE | ENM_MOUSEEVENTS );
    SetWindowSubclass( m_entry_view->m_richedit->m_hwnd, richedit_proc, 0, 0 );

    // TREE VIEW
    m_list =
            CreateWindowExW( 0, WC_TREEVIEW, L"",
                             WS_CHILD | WS_VISIBLE | WS_VSCROLL | TVS_HASLINES | TVS_HASBUTTONS |
                             TVS_FULLROWSELECT | TVS_EDITLABELS,
                             CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                             m_hwnd, ( HMENU ) IDLV_MAIN, GetModuleHandle( NULL ), NULL );
    init_list();

    // CALENDAR
    m_calendar =
            CreateWindowEx( 0, L"SysMonthCal32", L"",
                            WS_CHILD|WS_VISIBLE | MCS_DAYSTATE|MCS_NOTODAY,
                            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                            m_hwnd, ( HMENU ) IDCAL_MAIN, Lifeograph::hInst, NULL );
    SendMessage( m_calendar, WM_SETFONT,
                 ( WPARAM ) GetStockObject( DEFAULT_GUI_FONT ), MAKELPARAM( TRUE, 0 ) );
                 
    SetWindowSubclass( m_calendar, calendar_proc, 0, 0 );
    SetClassLongPtr( m_calendar, GCL_STYLE, CS_DBLCLKS ); // make it accept double clicks

    SYSTEMTIME lt;
    GetLocalTime( &lt );
    MonthCal_SetToday( m_calendar, &lt );
    
    m_login->handle_start();
}

void
WinAppWindow::handle_resize( short width, short height )
{
    RECT rc;
    MonthCal_GetMinReqRect( m_calendar, &rc );
    
    const int editor_width = width * EDITOR_RATIO;

    MoveWindow( m_toolbar, 0, 0, width, TOOLBAR_HEIGHT, TRUE );

    m_entry_view->m_tag_widget->handle_resize( editor_width, height );

    const int editor_height = height - m_entry_view->m_tag_widget->get_height() - TOOLBAR_HEIGHT;

    MoveWindow( m_entry_view->m_richedit->m_hwnd, 0, TOOLBAR_HEIGHT, editor_width, editor_height,
                TRUE );

    MoveWindow( m_list,
                editor_width, TOOLBAR_HEIGHT,
                width - editor_width, height - rc.bottom - TOOLBAR_HEIGHT,
                TRUE );

    MoveWindow( m_calendar,
                editor_width, height - rc.bottom, width - editor_width, rc.bottom,
                TRUE );

    ListView_SetColumnWidth( m_list, 0, width - editor_width - GetSystemMetrics( SM_CXVSCROLL ) );

    rc.left = 0;
    rc.top = editor_height;
    rc.right = editor_width;
    rc.bottom = height;
    InvalidateRect( m_hwnd, &rc, true );
}

BOOL
WinAppWindow::handle_notify( int id, LPARAM lparam )
{
    switch( id )
    {
        case IDLV_MAIN:
        {
            bool flag_rclick( false );

            switch( ( ( LPNMHDR ) lparam )->code )
            {
                case NM_RCLICK:
                    flag_rclick = true;
                case NM_CLICK:
                {
                    DWORD dwpos = GetMessagePos();
                    TVHITTESTINFO ht = { 0 };
                    ht.pt.x = GET_X_LPARAM( dwpos );
                    ht.pt.y = GET_Y_LPARAM( dwpos );
                    MapWindowPoints( HWND_DESKTOP, m_list, &ht.pt, 1 );

                    TreeView_HitTest( m_list, &ht );

                    if( ht.flags & TVHT_ONITEM )
                    {
                        TVITEM tvi;
                        tvi.mask        = TVIF_PARAM;
                        tvi.hItem       = ht.hItem;
                        if( TreeView_GetItem( m_list, &tvi ) )
                        {
                            SendMessage( m_list, TVM_SELECTITEM, TVGN_CARET, ( LPARAM ) ht.hItem );
                            DiaryElement* elem = Diary::d->get_element( tvi.lParam );
                            if( !elem )
                            {
                                if( DEID( tvi.lParam ) == Diary::d->get_id() )
                                    elem = Diary::d;
                                else
                                    break;
                            }
                            switch( elem->get_type() )
                            {
                                case DiaryElement::ET_ENTRY:
                                    elem->show();
                                    break;
                                case DiaryElement::ET_DIARY:
                                    if( !flag_rclick )
                                        break;
                                    display_context_menu( m_hwnd,
                                                          GET_X_LPARAM( dwpos ),
                                                          GET_Y_LPARAM( dwpos ),
                                                          IDM_DIARY_R );
                                    break;
                                case DiaryElement::ET_CHAPTER:
                                    if( !flag_rclick )
                                        break;
                                    display_context_menu( m_hwnd,
                                                          GET_X_LPARAM( dwpos ),
                                                          GET_Y_LPARAM( dwpos ),
                                                          IDM_CHAPTER );
                                    break;
                                case DiaryElement::ET_GROUP:
                                case DiaryElement::ET_TOPIC:
                                    if( !flag_rclick )
                                        break;
                                    display_context_menu( m_hwnd,
                                                          GET_X_LPARAM( dwpos ),
                                                          GET_Y_LPARAM( dwpos ),
                                                          IDM_TOPIC );
                                    break;
                                default:
                                    break;
                            }
                        }
                    }
                    break;
                }
                case TVN_ITEMEXPANDED:
                {
                    TVITEM tvi = ( ( LPNMTREEVIEW ) lparam )->itemNew;
                    DiaryElement* elem = Diary::d->get_element( tvi.lParam );
                    if( elem && ( elem->get_type() == DiaryElement::ET_CHAPTER ||
                                  elem->get_type() == DiaryElement::ET_TOPIC ||
                                  elem->get_type() == DiaryElement::ET_GROUP ) )
                    {
                        Chapter* chapter = dynamic_cast< Chapter* >( elem );
                        chapter->set_expanded( tvi.state & TVIS_EXPANDED );
                    }
                    break;
                }
                case TVN_BEGINLABELEDIT:
                {
                    DiaryElement* elem = get_selected_list_elem();
                    if( elem && ( elem->get_type() == DiaryElement::ET_CHAPTER ||
                                  elem->get_type() == DiaryElement::ET_TOPIC ||
                                  elem->get_type() == DiaryElement::ET_GROUP ) )
                    {
                        m_list_edit_item = TreeView_GetEditControl( m_list );

                        SetWindowText( m_list_edit_item,
                                       HELPERS::convert_utf8_to_16( elem->get_name() ) );
                    }
                    else
                        return TRUE;
                    break;
                }
                case TVN_ENDLABELEDIT:
                {
                    wchar_t buffer[ 256 ] = L"";
                    GetWindowText( m_list_edit_item, buffer, sizeof( buffer ) );
                    if( wcslen( buffer ) > 0 )
                    {
                        DiaryElement* elem = get_selected_list_elem();
                        if( elem )
                        {
                            elem->set_name( HELPERS::convert_utf16_to_8( buffer ) );
                            update_list_elem( elem );
                        }
                    }
                }
            }
            break;
        }
        case IDRT_MAIN:
        {
            MSGFILTER* lpMsgFilter = ( MSGFILTER* ) lparam;
            DWORD dwpos = GetMessagePos();
            if( ( lpMsgFilter->nmhdr.code == EN_MSGFILTER ) &&
                ( lpMsgFilter->msg == WM_RBUTTONDOWN ) )
            {
                //m_entry_view->m_richedit->handle_right_click();
                display_context_menu( m_hwnd,
                                      GET_X_LPARAM( dwpos ),
                                      GET_Y_LPARAM( dwpos ),
                                      IDM_TEXT_EDITOR );
            }
            break;
        }
        case IDCAL_MAIN:
            if( ( ( LPNMHDR ) lparam )->code == MCN_GETDAYSTATE )
            {
                NMDAYSTATE* ds( ( NMDAYSTATE* ) lparam );
                MONTHDAYSTATE mds[ 12 ] = { 0 };

                fill_monthdaystate( ds->stStart.wYear,
                                    ds->stStart.wMonth,
                                    mds,
                                    ds->cDayState );

                ds->prgDayState = mds;
            }
            else if( ( ( LPNMHDR ) lparam )->code == MCN_SELECT )
            {
                if( Lifeograph::loginstatus != Lifeograph::LOGGED_IN )
                    break;
                //if( Lifeograph::m_internaloperation ) return;

                NMSELCHANGE* sc = ( LPNMSELCHANGE ) lparam;
                m_date_selected = Date::make_date( sc->stSelStart.wYear,
                                                   sc->stSelStart.wMonth,
                                                   sc->stSelStart.wDay );
                Entry* entry;

                if( m_entry_view->get_element() &&
                    m_date_selected == ( m_entry_view->get_element()->get_date().get_pure() ) )
                {
                    entry = Diary::d->get_entry_next_in_day(
                            m_entry_view->get_element()->get_date() );
                }
                else
                {
                    entry = Diary::d->get_entry( m_date_selected + 1 ); // 1 increment to fix order
                }

                if( entry )
                    entry->show();
            }
            break;
    }
    
    return FALSE;
}

// LOG OUT
bool
WinAppWindow::finish_editing( bool opt_save )
{
    // files added to recent list here if not already there
    if( ! Diary::d->get_path().empty() )
        if( Lifeograph::stock_diaries.find( Diary::d->get_path() ) ==
                Lifeograph::stock_diaries.end() )
            Lifeograph::settings.recentfiles.insert( Diary::d->get_path() );

    if( ! Diary::d->is_read_only() )
    {
        m_entry_view->sync();

        if( m_entry_view->get_element() )
            Diary::d->set_last_elem( m_entry_view->get_element() );

        if( opt_save )
        {
            if( Diary::d->write() != SUCCESS )
            {
                MessageBoxA( m_hwnd,
                             STRING::CANNOT_WRITE_SUB,
                             STRING::CANNOT_WRITE,
                             MB_OK|MB_ICONERROR );
                return false;
            }
        }
        else
        {
            if( MessageBoxA( m_hwnd,
                             "Your changes will be backed up in .~unsaved~."
                             "If you exit normally, your diary is saved automatically.",
                             "Are you sure you want to log out without saving?",
                             MB_YESNO | MB_ICONQUESTION ) != IDYES )
                return false;
            // back up changes
            Diary::d->write( Diary::d->get_path() + ".~unsaved~" );
        }
    }

    if( ! Diary::d->is_read_only() )
        KillTimer( m_hwnd, m_backup_timer_id );

    // CLEARING
    // TODO: m_loginstatus = LOGGING_OUT_IN_PROGRESS;

    Lifeograph::m_internaloperation++;
//    panel_main->handle_logout();
//    panel_diary->handle_logout();
//    m_entry_view->get_buffer()->handle_logout();
//    panel_extra->handle_logout();
    SetWindowText( m_edit_search, L"" );

    ShowWindow( m_button_today, SW_HIDE );
    ShowWindow( m_button_elem, SW_HIDE );
    ShowWindow( m_button_title, SW_HIDE );
    ShowWindow( m_button_filter, SW_HIDE );
    ShowWindow( m_edit_search, SW_HIDE );

    ShowWindow( m_button_about, SW_SHOW );

    if( Lifeograph::loginstatus == Lifeograph::LOGGED_IN )
        Lifeograph::loginstatus = Lifeograph::LOGGED_OUT;
        
    Diary::d->clear();
    m_entry_view->clear();
    TreeView_DeleteAllItems( m_list );
    update_calendar();
    update_title();

    Lifeograph::m_internaloperation--;

    return true;
}

void
WinAppWindow::logout( bool opt_save )
{
    Lifeograph::p->m_flag_open_directly = false;   // should be reset to prevent logging in again
    if( finish_editing( opt_save ) )
        m_login->handle_logout();

    m_auto_logout_status = 1;
}

void
WinAppWindow::login()
{
    // LOGIN
    // the following must come before handle_login()s
    m_auto_logout_status = ( Lifeograph::settings.autologout && Diary::d->is_encrypted() ) ? 0 : 1;

    Lifeograph::m_internaloperation++;

//    panel_main->handle_login(); // must come before m_diary_view->handle_login() for header bar order
//    m_view_login->handle_login();
//    m_tag_view->handle_login();
//    m_filter_view->handle_login();
    m_entry_view->handle_login();
//    m_diary_view->handle_login();
//    m_chapter_view->handle_login();
//    panel_diary->handle_login();
//    panel_extra->handle_login();

    update_entry_list();
    update_calendar();

    Lifeograph::m_internaloperation--;

    Lifeograph::loginstatus = Lifeograph::LOGGED_IN;
    
    if( ! Diary::d->is_read_only() )
        m_backup_timer_id = SetTimer( m_hwnd, ID_TIMER_BACKUP, BACKUP_INTERVAL * 1000, nullptr );

    update_title();

    ShowWindow( m_button_about, SW_HIDE );
    ShowWindow( m_button_today, SW_SHOW );
    ShowWindow( m_button_filter, SW_SHOW );
    ShowWindow( m_edit_search, SW_SHOW );

    DiaryElement* startup_elem = Diary::d->get_startup_elem();
    
    if( startup_elem->get_type() == DiaryElement::ET_ENTRY )
        startup_elem->show();
}

VOID APIENTRY
WinAppWindow::display_context_menu( HWND hwnd, int x, int y, int id )
{
    if( ( m_hmenu = LoadMenu( Lifeograph::hInst, MAKEINTRESOURCE( id ) ) ) == NULL )
        return;

    // TrackPopupMenu cannot display the menu bar so get a handle to the first shortcut menu
    HMENU hmenuTrackPopup = GetSubMenu( m_hmenu, 0 );

    // Display the shortcut menu. Track the right mouse button
    TrackPopupMenu( hmenuTrackPopup, TPM_LEFTALIGN, x, y, 0, hwnd, NULL );

    DestroyMenu( m_hmenu );
}

void
WinAppWindow::update_title()
{
    Ustring title( PROGRAM_NAME );

    if( Lifeograph::loginstatus == Lifeograph::LOGGED_IN )
    {
        title += " - ";
        title += Diary::d->get_name();

        if( Diary::d->is_read_only() )
            title += " <Read Only>";
    }

    SetWindowTextA( m_hwnd, title.c_str() );
}

void
WinAppWindow::update_elem_icon()
{
    int id = 1;
    
    switch( m_entry_view->get_element()->get_icon() )
    {
        case 1:
            id = IDI_ENTRY16;
            break;
        case 2:
            id = IDI_TODO_OPEN16;
            break;
        case 3:
            id = IDI_TODO_PROG16;
            break;
        case 4:
            id = IDI_TODO_DONE16;
            break;
        case 5:
            id = IDI_TODO_CANC16;
            break;
    }
    HICON ico = LoadIcon( GetModuleHandle( NULL ), MAKEINTRESOURCE( id ) );
    WAO_set_icon( m_button_elem, ico );
}

void
WinAppWindow::update_menu()
{
    bool logged_in = Lifeograph::loginstatus == Lifeograph::LOGGED_IN;
    bool encrypted = Diary::d->is_encrypted();
    
    EnableMenuItem( m_hmenu, IDMI_DIARY_ENCRYPT,
                    MF_BYCOMMAND | ( logged_in && !encrypted ? MF_ENABLED : MF_GRAYED ) );
                    
    EnableMenuItem( m_hmenu, IDMI_DIARY_CHANGE_PASSWORD,
                    MF_BYCOMMAND | ( logged_in && encrypted ? MF_ENABLED : MF_GRAYED ) );

    EnableMenuItem( m_hmenu, IDMI_DIARY_EXPORT,
                    MF_BYCOMMAND | ( logged_in ? MF_ENABLED : MF_GRAYED ) );
    EnableMenuItem( m_hmenu, IDMI_QUIT_WO_SAVE,
                    MF_BYCOMMAND | ( logged_in ? MF_ENABLED : MF_GRAYED ) );
                            
    if( logged_in )
    {
        if( m_entry_view->get_element() )
        {
            ElemStatus status = m_entry_view->get_element()->get_status();
            CheckMenuItem( m_hmenu, IDMI_ENTRY_TODO_NOT,
                           MF_BYCOMMAND | ( ( status & ES::NOT_TODO ) ? MF_CHECKED : MF_UNCHECKED ) );
            CheckMenuItem( m_hmenu, IDMI_ENTRY_TODO_OPEN,
                           MF_BYCOMMAND | ( ( status & ES::TODO ) ? MF_CHECKED : MF_UNCHECKED ) );
            CheckMenuItem( m_hmenu, IDMI_ENTRY_TODO_PROGRESSED,
                           MF_BYCOMMAND | ( ( status & ES::PROGRESSED ) ? MF_CHECKED : MF_UNCHECKED ) );
            CheckMenuItem( m_hmenu, IDMI_ENTRY_TODO_DONE,
                           MF_BYCOMMAND | ( ( status & ES::DONE ) ? MF_CHECKED : MF_UNCHECKED ) );
            CheckMenuItem( m_hmenu, IDMI_ENTRY_TODO_CANCELED,
                           MF_BYCOMMAND | ( ( status & ES::CANCELED ) ? MF_CHECKED : MF_UNCHECKED ) );

            CheckMenuItem( m_hmenu, IDMI_ENTRY_FAVORITE,
                           MF_BYCOMMAND | ( m_entry_view->get_element()->is_favored() ?
                                                MF_CHECKED : MF_UNCHECKED ) );
        }

        EnableMenuItem( m_hmenu, IDMI_DIARY_ADD_DCHAPTER,
                        MF_BYCOMMAND |
                        ( Diary::d->get_current_chapter_ctg()->get_chapter( m_date_selected ) ?
                            MF_GRAYED : MF_ENABLED ) );
                            
        ElemStatus fs = Diary::d->get_filter_status();
        CheckMenuItem( m_hmenu, IDMI_FILTER_TODO_NOT,
                       MF_BYCOMMAND | ( ( fs & ES::SHOW_NOT_TODO ) ? MF_CHECKED : MF_UNCHECKED ) );
        CheckMenuItem( m_hmenu, IDMI_FILTER_TODO_OPEN,
                       MF_BYCOMMAND | ( ( fs & ES::SHOW_TODO ) ? MF_CHECKED : MF_UNCHECKED ) );
        CheckMenuItem( m_hmenu, IDMI_FILTER_TODO_PROGRESSED,
                       MF_BYCOMMAND | ( ( fs & ES::SHOW_PROGRESSED ) ?
                                MF_CHECKED : MF_UNCHECKED ) );
        CheckMenuItem( m_hmenu, IDMI_FILTER_TODO_DONE,
                       MF_BYCOMMAND | ( ( fs & ES::SHOW_DONE ) ? MF_CHECKED : MF_UNCHECKED ) );
        CheckMenuItem( m_hmenu, IDMI_FILTER_TODO_CANCELED,
                       MF_BYCOMMAND | ( ( fs & ES::SHOW_CANCELED ) ? MF_CHECKED : MF_UNCHECKED ) );

        CheckMenuItem( m_hmenu, IDMI_FILTER_FAVORITES,
                       MF_BYCOMMAND | ( ( fs & ES::SHOW_FAVORED ) && !( fs & ES::SHOW_NOT_FAVORED )
                                ? MF_CHECKED : MF_UNCHECKED ) );
    }
}

BOOL
WinAppWindow::init_list()
{
    SendMessage( m_list, WM_SETFONT,
                 ( WPARAM ) GetStockObject( DEFAULT_GUI_FONT ), MAKELPARAM( TRUE, 0 ) );
    //ListView_SetExtendedListViewStyle( m_list, LVS_EX_FULLROWSELECT );
    //ListView_SetUnicodeFormat( m_list, true );

    // COLUMNS
    /*LV_COLUMN lvc;

    lvc.mask     = LVCF_FMT | LVCF_TEXT;
    lvc.fmt      = LVCFMT_LEFT | LVCFMT_IMAGE;
    lvc.pszText  = ( wchar_t* ) L"Entries";

    ListView_InsertColumn( m_list, 0, &lvc );*/

    // IMAGE LISTS
    /*HIMAGELIST himagelist = ImageList_Create( 16, 16, ILC_MASK, 1, 1 );

    for( int i = 0; i < 5; i++ )
    {
        HICON hicon = LoadIcon( Lifeograph::p->hInst, MAKEINTRESOURCE( IDI_ENTRY16 + i ) );
        ImageList_AddIcon( himagelist, hicon );
        //DestroyIcon( hicon );
    }*/

    HIMAGELIST himagelist = ImageList_Create( 16, 16, ILC_COLOR24, 0, 12 );
    HBITMAP hbitmap = ( HBITMAP ) LoadImage( Lifeograph::p->hInst,
                                             MAKEINTRESOURCE( IDBM_ENTRY16 ),
                                             IMAGE_BITMAP, 0, 0,
                                             LR_LOADTRANSPARENT );

    ImageList_Add( himagelist, hbitmap, NULL );

    DeleteObject( hbitmap );

    TreeView_SetImageList( m_list, himagelist, TVSIL_NORMAL );

    return TRUE;
}

bool
WinAppWindow::select_list_elem( const DiaryElement* elem )
{
    TreeView_SelectItem( m_list, elem->m_list_data );
    TreeView_EnsureVisible( m_list, elem->m_list_data );

    return true;
}

bool
WinAppWindow::update_list_elem( const DiaryElement* elem )
{
    TVITEM tvi;
    tvi.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
    tvi.pszText = HELPERS::convert_utf8_to_16( elem->get_list_str() );
    tvi.cchTextMax = sizeof( tvi.pszText ) / sizeof( tvi.pszText[ 0 ] );
    tvi.iImage = tvi.iSelectedImage = elem->get_icon();
    tvi.lParam = ( LPARAM ) elem->get_id();
    tvi.hItem = elem->m_list_data;

    TreeView_SetItem( m_list, &tvi );
    TreeView_EnsureVisible( m_list, elem->m_list_data );

    return true;
}

HTREEITEM
WinAppWindow::add_list_elem( DiaryElement* elem, HTREEITEM hti_root, bool flag_bold )
{
    TVITEM tvi;
    TVINSERTSTRUCT tvins;
    HTREEITEM hPrev = ( HTREEITEM ) TVI_FIRST;

    tvi.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
    tvi.pszText = HELPERS::convert_utf8_to_16( elem->get_list_str() );
    tvi.cchTextMax = sizeof( tvi.pszText ) / sizeof( tvi.pszText[ 0 ] );
    tvi.iImage = tvi.iSelectedImage = elem->get_icon();
    tvi.lParam = ( LPARAM ) elem->get_id();
    
    if( flag_bold )
    {
        tvi.mask |= TVIF_STATE;
        tvi.state = TVIS_BOLD;
    }

    tvins.item = tvi;
    tvins.hInsertAfter = hPrev;
    tvins.hParent = hti_root;

    elem->m_list_data = ( HTREEITEM ) SendMessage( m_list,
                                                   TVM_INSERTITEM,
                                                   0,
                                                   ( LPARAM ) ( LPTVINSERTSTRUCT ) &tvins );

    return elem->m_list_data;
}

void
WinAppWindow::add_chapter_category_to_list( const CategoryChapters* chapters, HTREEITEM hti_root )
{
    for( auto& kv_chapter : *chapters )
    {
        Chapter* chapter( kv_chapter.second );

        HTREEITEM hti_chapter = add_list_elem( chapter, hti_root, true );

        for( Entry* entry : *chapter )
        {
            if( entry->get_filtered_out() == false )
            {
                add_list_elem( entry, hti_chapter );
            }
        }

        if( chapter->get_expanded() )
            TreeView_Expand( m_list, hti_chapter, TVE_EXPAND );

        TVSORTCB tvscb;
        tvscb.hParent = hti_chapter;
        tvscb.lpfnCompare = list_compare_func;
        tvscb.lParam = 0;
        SendMessage( m_list, TVM_SORTCHILDRENCB, 0, ( WPARAM ) &tvscb );
    }
}

void
WinAppWindow::update_entry_list()
{
    SendMessage( m_list, TVM_DELETEITEM, 0, ( LPARAM ) NULL );

    // DIARY ITEM
    HTREEITEM hti_diary = add_list_elem( Diary::d, ( HTREEITEM ) TVI_ROOT, true );

    // CHAPTERS
    if( Diary::d->get_sorting_criteria() == SC_DATE )
    {
        add_chapter_category_to_list( Diary::d->get_topics(), hti_diary );
        add_chapter_category_to_list( Diary::d->get_groups(), hti_diary );
        add_chapter_category_to_list( Diary::d->get_current_chapter_ctg(), hti_diary );

        // ENTRIES NOT PART OF A CHAPTER (ORPHANED)
        for( Entry* entry : *Diary::d->get_orphans() )
        {
            if( entry->get_filtered_out() == false )
            {
                add_list_elem( entry, hti_diary );
            }
        }
    }
    else
    {
        for( auto& kv_entry : Diary::d->get_entries() )
        {
            Entry* entry( kv_entry.second );
            if( entry->get_filtered_out() == false )
            {
                add_list_elem( entry, hti_diary );
            }

        }
    }

    // always expand diary
    SendMessage( m_list, TVM_EXPAND, TVE_EXPAND, ( WPARAM ) hti_diary );

    TVSORTCB tvscb;
    tvscb.hParent = hti_diary;
    tvscb.lpfnCompare = list_compare_func;
    tvscb.lParam = 0;
    SendMessage( m_list, TVM_SORTCHILDRENCB, 0, ( WPARAM ) &tvscb );
}

DiaryElement*
WinAppWindow::get_selected_list_elem()
{
    HTREEITEM item = TreeView_GetSelection( m_list );
    TVITEM tvi;
    tvi.mask        = TVIF_PARAM;
    tvi.hItem       = item;
    if( TreeView_GetItem( m_list, &tvi ) )
        return Diary::d->get_element( tvi.lParam );
    else
        return NULL;
}

#define BOLDDAY(ds, iDay)  if(iDay > 0 && iDay < 32) (ds) |= (0x00000001 << (iDay - 1))

void
WinAppWindow::fill_monthdaystate( int year, int month, MONTHDAYSTATE mds[], int size )
{
    for( int i = 0; i < size; i++ )
    {
        for( auto& kv_entry : Diary::d->get_entries() )
        {
            Entry* entry = kv_entry.second;

            if( entry->get_filtered_out() || entry->get_date().is_ordinal() )
                continue;

            if( int( entry->get_date().get_year() ) == year )
            {
                if( int( entry->get_date().get_month() ) == month )
                {
                    BOLDDAY( mds[ i ], entry->get_date().get_day() );
                }
                else if( int( entry->get_date().get_month() ) < month )
                {
                    break;
                }
            }
            else
            if( int( entry->get_date().get_year() ) < year )
                break;
        }

        // increase month and, if needed, year
        if( ++month > 12 )
        {
            month = 1;
            year++;
        }
    }
}

void
WinAppWindow::update_calendar()
{
    SYSTEMTIME st[ 2 ];
    MONTHDAYSTATE mds[ 12 ] = { 0 };
    int size = SendMessage( m_calendar, MCM_GETMONTHRANGE, GMR_DAYSTATE, ( LPARAM ) st );

    fill_monthdaystate( st[ 0 ].wYear, st[ 0 ].wMonth, mds, size );

    SendMessage( m_calendar, MCM_SETDAYSTATE, size, ( LPARAM ) &mds );

    RECT rect;
    
    MonthCal_GetMinReqRect( m_calendar, &rect );
    const int height = rect.bottom - rect.top;
    GetClientRect( m_hwnd, &rect );
    
    rect.left = rect.right * EDITOR_RATIO;
    rect.top = rect.bottom  - height;
    
    InvalidateRect( m_hwnd, &rect, TRUE );
}

void
WinAppWindow::handle_calendar_doubleclick( const POINT& pt )
{
    if( Lifeograph::loginstatus != Lifeograph::LOGGED_IN || Diary::d->is_read_only() )
        return;

    MCHITTESTINFO cal_ht = { 0 };
    cal_ht.cbSize = sizeof( MCHITTESTINFO );
    cal_ht.pt = pt;
    if( MonthCal_HitTest( m_calendar, &cal_ht ) != MCHT_CALENDARDATE )
        return;

    SYSTEMTIME st;
    if( ! SendMessage( m_calendar, MCM_GETCURSEL, 0, ( LPARAM ) &st ) )
        return;

    Date date( st.wYear, st.wMonth, st.wDay );
    Entry* entry( Diary::d->create_entry( date.m_date ) );

    update_entry_list();
    update_calendar();

    entry->show();
}

void
WinAppWindow::show_today()
{
    if( Lifeograph::p->loginstatus != Lifeograph::LOGGED_IN  )
        return;

    Entry* entry_today = Diary::d->get_entry_today();
    bool flag_add = false;

    if( entry_today == NULL )   // add new entry if no entry exists on selected date
    {
        if( Diary::d->is_read_only() )
            return;
        else
            flag_add = true;
    }
    else                        // or current entry is already at today
    if( entry_today->get_date().get_pure() == m_entry_view->get_element()->get_date().get_pure() )
        flag_add = true;

    if( flag_add )
    {
        entry_today = Diary::d->add_today();
        update_entry_list();
    }

    entry_today->show();
}

void
WinAppWindow::add_todo_entry()
{
    if( Lifeograph::p->loginstatus != Lifeograph::LOGGED_IN || Diary::d->is_read_only() )
        return;

    Entry* entry = Diary::d->add_today();
    if( entry )
    {
        entry->set_todo_status( ES::TODO );
        update_entry_list();
        entry->show();
    }
}

void
WinAppWindow::show_about()
{
    Ustring about_txt = "Lifeograph for Windows ";
    about_txt += Lifeograph::PROGRAM_VERSION_STRING;
    about_txt += "\n\nCopyright (C) 2014-2017 Ahmet �zt�rk\n"
                 "Lifeograph is licensed under GNU Public License v3\n\n"
                 "http://lifeograph.sourceforge.net",
    MessageBoxA( NULL, about_txt.c_str(), "About...", MB_OK );
}

bool
WinAppWindow::write_backup()
{
    m_entry_view->sync();

    Result result{ Diary::d->write( Diary::d->get_path() + LOCK_SUFFIX ) };
    return( result == SUCCESS );
}

void
WinAppWindow::add_chapter( CategoryChapters* cc, const Date* d )
{
    Chapter* c =
        d ?
            cc->create_chapter( "New chapter", d->m_date ) :
            cc->create_chapter_ordinal( "New chapter" );

    Diary::d->update_entries_in_chapters();
    update_entry_list();
    select_list_elem( c );
    HTREEITEM item = TreeView_GetSelection( m_list );
    if( item )
        TreeView_EditLabel( m_list, item );
}

void
WinAppWindow::start_tag_dialog( const Ustring& name )
{
    if( DialogTags::launch( m_hwnd, Diary::d, m_entry_view->get_element(), name ) == SUCCESS )
    {
        m_entry_view->update_theme();
        m_entry_view->m_tag_widget->update_full();
    }
}

bool
WinAppWindow::authorize()
{
    int i = 0;
    Result res;
    
    while( ( res = DialogPassword::launch( m_hwnd, Diary::d, DialogPassword::PD_AUTHORIZE, i ) ) )
    {
        switch( res )
        {
            case SUCCESS:
                return true;
            case INVALID:
                i++;
                break;
            case ABORTED:
                return false;
            default:
                return false;
        }
    }
    
    return false;
}

bool
WinAppWindow::confirm_dismiss_element( const DiaryElement* elem )
{
    return(
        MessageBox( m_hwnd,
                    convert_utf8_to_16(
                            STR::compose( "Are you sure, you want to dismiss ",
                                          elem->get_name() ) ),
                    L"Confirm Dismiss",
                    MB_YESNO | MB_ICONWARNING ) == IDYES );
}

void
WinAppWindow::handle_search_string_changed()
{
    if( Lifeograph::m_internaloperation ) return;
    // current entry must be closed here or else it loses...
    // ...changes since it was selected:
    //AppWindow::p->m_entry_view->sync(); update_entry_list() does the syncing

    int size( GetWindowTextLength( m_edit_search ) + 1 );
    wchar_t* buffer = new wchar_t[ size ];
    GetWindowText( m_edit_search, buffer, size );
    const Wstring filterstring( buffer );

    Diary::d->set_search_text( HELPERS::convert_utf16_to_8( buffer ) );
    update_entry_list();    // syncs current entry

    if( filterstring.size() > 0 )
    {
        m_entry_view->m_richedit->set_search_str( filterstring );

        /*if( panel_diary->get_entry_count() > 0 )
        {
            if( AppWindow::p->panel_main->get_cur_elem_type() == DiaryElement::ET_ENTRY )
                AppWindow::p->m_entry_view->get_buffer()->select_searchstr_next();
        }*/

        /*m_E_search->set_tooltip_markup(
                Glib::ustring::compose(
                        _( "Found in <b>%1</b> of <b>%2</b> entrie(s)" ),
                        AppWindow::p->panel_diary->get_entry_count(), Diary::d->get_size() ) );*/
    }
    else
    {
        m_entry_view->m_richedit->set_search_str( L"" );
        //m_E_search->set_tooltip_text( _( "Search text within entries" ) );
    }

    update_calendar();

    delete buffer;
}

