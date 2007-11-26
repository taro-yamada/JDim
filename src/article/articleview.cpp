// ライセンス: GPL2

//#define _DEBUG
#include "jddebug.h"

#include "articleadmin.h"
#include "articleview.h"
#include "drawareamain.h"
#include "toolbar.h"

#include "skeleton/msgdiag.h"

#include "dbtree/interface.h"
#include "dbtree/articlebase.h"

#include "jdlib/miscutil.h"
#include "jdlib/misctime.h"

#include "config/globalconf.h"

#include "command.h"
#include "global.h"
#include "httpcode.h"
#include "session.h"
#include "controlid.h"

#include <sstream>
#include <sys/time.h>


using namespace ARTICLE;


// メインビュー

ArticleViewMain::ArticleViewMain( const std::string& url )
    :  ArticleViewBase( url ), m_gotonum_reserve( 0 )
{
#ifdef _DEBUG
    std::cout << "ArticleViewMain::ArticleViewMain " << get_url() << " url_article = " << url_article() << std::endl;
#endif

    // オートリロード可
    set_enable_autoreload( true );

    setup_view();
}



ArticleViewMain::~ArticleViewMain()
{
#ifdef _DEBUG    
    std::cout << "ArticleViewMain::~ArticleViewMain : " << get_url() << " url_article = " << url_article() << std::endl;
#endif
    int seen = drawarea()->get_seen_current();
        
#ifdef _DEBUG    
    std::cout << "set seen to " << seen << std::endl;
#endif

    if( seen >= 1 ) get_article()->set_number_seen( seen );

    // 閉じたタブ履歴更新
    CORE::core_set_command( "set_history_close", url_article() );
}


//
// num 番にジャンプ
//
// ローディング中ならジャンプ予約をしてロード後に update_finish() の中で改めて goto_num() を呼び出す
//
void ArticleViewMain::goto_num( int num )
{
    if( get_article()->get_number_load() < num  && is_loading() ){

        m_gotonum_reserve = num;
        return;
    }

    m_gotonum_reserve = 0;
    ArticleViewBase::goto_num( num );
}


// ロード中
const bool ArticleViewMain::is_loading()
{
    return get_article()->is_loading();
}


// 更新した
const bool ArticleViewMain::is_updated()
{
    int code = DBTREE::article_code( url_article() );
    return ( code == HTTP_OK || code == HTTP_PARTIAL_CONTENT );
}


// 更新チェックして更新可能か
const bool ArticleViewMain::is_check_update()
{
    return ( get_article()->get_status() & STATUS_UPDATE );
}

// 古いデータか
const bool ArticleViewMain::is_old()
{
    return ( get_article()->get_status() & STATUS_OLD );
}

// 壊れているか
const bool ArticleViewMain::is_broken()
{
    return ( get_article()->get_status() & STATUS_BROKEN );
}

//
// 再読み込み
//
void ArticleViewMain::reload()
{
    // オフライン
    if( ! SESSION::is_online() ){
        SKELETON::MsgDiag mdiag( NULL, "オフラインです" );
        mdiag.run();
        return;
    }

    // オートリロードのカウンタを0にする
    View::reset_autoreload_counter();

    show_view();

    // スレ履歴更新
    CORE::core_set_command( "set_history_article", url_article() );
}



//
//  キャッシュ表示 & 差分ロード開始
//
void ArticleViewMain::show_view()
{
    m_gotonum_reserve = 0;
    m_show_instdialog = false;

#ifdef _DEBUG
    std::cout << "ArticleViewBase::show_view\n";
#endif

    if( get_url().empty() ){
        set_status( "invalid URL" );
        ARTICLE::get_admin()->set_command( "set_status", get_url(), get_status() );
        return;
    }

    // もしarticleクラスがまだキャッシュにあるdatを解析していないときに
    // drawarea()->append_res()を呼ぶと update_finish() がコールバック
    // されて2回再描画することになるので、 show_view() の中で update_finish()を
    // 呼ばないようにする。動作をまとめると次のようになる。

    // オフライン　かつ
    //   キャッシュを読み込んでいない場合  -> articleでnodetreeが作られた時に update_finish がコールバックされる
    //   キャッシュを読み込んでいる場合    -> show_viewから直接  update_finish を呼ぶ
    //
    // オンライン　かつ
    //   キャッシュを読み込んでいない場合  -> articleでnodetreeが作られた時に update_finish がコールバックされる
    // 　　　　　　　　　　　　　　　　　　　　ロード終了時にもupdate_finish がコールバックされる
    //   キャッシュを読み込んでいる場合    -> show_viewから直接  update_finish を呼ぶ
    //  　　　　　　　　　　　　　　　　　　　　ロード終了時にもupdate_finish がコールバックされる

    bool call_update_finish = get_article()->is_cache_read();

    // キャッシュに含まれているレスを表示
    int from_num = drawarea()->max_number() + 1;
    int to_num = get_article()->get_number_load();
    if( from_num <= to_num ){

        drawarea()->append_res( from_num, to_num );

        // 以前見ていたところにジャンプ
        drawarea()->goto_num( get_article()->get_number_seen() );
    }

    // セパレータを最後に移動
    drawarea()->set_separator_new( to_num + 1 );

    // update_finish() を呼んでキャッシュの分を描画
    if( call_update_finish ){

        // update_finish()後に一番最後や新着にジャンプしないように設定を一時的に解除する
        const bool jump_bottom = CONFIG::get_jump_after_reload();
        const bool jump_new = CONFIG::get_jump_new_after_reload();
        CONFIG::set_jump_after_reload( false );
        CONFIG::set_jump_new_after_reload( false );

        update_finish();

        CONFIG::set_jump_after_reload( jump_bottom );
        CONFIG::set_jump_new_after_reload( jump_new );
    }

    // オフラインならダウンロードを開始しない
    if( ! SESSION::is_online() ) return;

    // 板一覧との切り替え方法説明ダイアログ表示
    if( CONFIG::get_instruct_tglart() && SESSION::get_mode_pane() == SESSION::MODE_2PANE ){
        m_show_instdialog = true;
    }

    slot_push_claar_hl();

    // 差分 download 開始
    get_article()->download_dat( false );
    if( is_loading() ){

        set_status( "loading..." );
        ARTICLE::get_admin()->set_command( "set_status", get_url(), get_status() );

        // タブのアイコン状態を更新
        ARTICLE::get_admin()->set_command( "toggle_icon", get_url() );
    }
}



//
// ロード中にノード構造が変わったら呼ばれる
//
void ArticleViewMain::update_view()
{
    int num_from = drawarea()->max_number() + 1;
    int num_to = get_article()->get_number_load();

#ifdef _DEBUG
    std::cout << "ArticleViewMain::update_view : from " << num_from << " to " << num_to << std::endl;
#endif

    if( num_from > num_to ) return;

    drawarea()->append_res( num_from, num_to );
    drawarea()->redraw_view();
}



//
// ロードが終わったときに呼ばれる
//
void ArticleViewMain::update_finish()
{
    std::string str_stat;
    if( is_old() ) str_stat = "[ DAT落ち 又は 移転しました ] ";
    if( is_check_update() ) str_stat += "[ 更新可能です ] ";
    if( is_broken() ) str_stat += "[ 壊れています ] ";

    if( ! DBTREE::article_ext_err( url_article() ).empty() ) str_stat += "[ " + DBTREE::article_ext_err( url_article() ) + " ] ";

    // 板名とスレ名をセット
    if( get_articletoolbar()->m_button_board.get_label().empty() ) get_articletoolbar()->m_button_board.set_label( "[ " + DBTREE::board_name( url_article() ) + " ]" );

    std::string str_tablabel;
    if( is_broken() ){
        get_articletoolbar()->set_broken();
        str_tablabel = "[ 壊れています ]  ";
    }
    else if( is_old() ){
        get_articletoolbar()->set_old();
        str_tablabel = "[ DAT落ち ]  ";
    }

    if( get_articletoolbar()->get_label().empty() || ! str_tablabel.empty() ) get_articletoolbar()->set_label( str_tablabel + DBTREE::article_subject( url_article() ) );

    // タブのラベルセット
    std::string str_label = DBTREE::article_subject( url_article() );
    ARTICLE::get_admin()->set_command( "set_tablabel", get_url(), str_label ); 

    // タブのアイコン状態を更新
    ARTICLE::get_admin()->set_command( "toggle_icon", get_url() );


#ifdef _DEBUG
    int code = DBTREE::article_code( url_article() );
    std::cout << "ArticleViewMain::update_finish " << str_label << " code = " << code << std::endl;;
#endif

    // 新着セパレータを消す
    int number_new = DBTREE::article_number_new( url_article() );
    if( ! number_new ) drawarea()->hide_separator_new();

    // ステータス表示
    std::ostringstream ss_tmp;
    ss_tmp << DBTREE::article_str_code( url_article() )
           << " [ 全 " << DBTREE::article_number_load( url_article() )
           << " / 新着 " << number_new;

    if( DBTREE::article_write_time( url_article() ) ) ss_tmp << " / 最終書込 " << DBTREE::article_write_date( url_article() );

    ss_tmp << " / 速度 " << DBTREE::article_get_speed( url_article() )
           << " / " << DBTREE::article_lng_dat( url_article() )/1024 << " k ] "
           << str_stat;

    set_status( ss_tmp.str() );
    ARTICLE::get_admin()->set_command( "set_status", get_url(), get_status() );

    // タイトルセット
    set_title( DBTREE::article_subject( url_article() ) );
    ARTICLE::get_admin()->set_command( "set_title", get_url(), get_title() );

    // 全体再描画
    drawarea()->redraw_view();

    if( CONFIG::get_jump_after_reload() ) goto_bottom();
    else if( number_new && CONFIG::get_jump_new_after_reload() ) goto_new();
    else if( m_gotonum_reserve ) goto_num( m_gotonum_reserve );
    m_gotonum_reserve = 0;

    if( m_show_instdialog ) show_instruct_diag();
}


//
// 板一覧との切り替え方法説明ダイアログ表示
//
void ArticleViewMain::show_instruct_diag()
{
    SKELETON::MsgDiag mdiag( NULL, 
        "スレビューからスレ一覧表示に戻る方法として\n\n(1) マウスジェスチャを使う( マウスの右ボタンを押しながら左または下にドラッグして右ボタンを離す )\n\n(2) マウスの5ボタンを押す\n\n(3) Alt+x か h か ← を押す\n\n(4) ツールバーのスレ一覧アイコンを押す\n\n(5) 表示メニューからスレ一覧を選ぶ\n\nなどがあります。詳しくはオンラインマニュアルを参照してください。" );
    Gtk::CheckButton chkbutton( "今後表示しない" );
    mdiag.get_vbox()->pack_start( chkbutton, Gtk::PACK_SHRINK );
    mdiag.set_title( "ヒント" );
    mdiag.show_all_children();
    mdiag.run();

    if( chkbutton.get_active() ) CONFIG::set_instruct_tglart( false );
    m_show_instdialog = false;
}



//
// 画面を消してレイアウトやりなおし & 再描画
//
void ArticleViewMain::relayout()
{
#ifdef _DEBUG
    std::cout << "ArticleViewMain::relayout\n";
#endif

    hide_popup( true );

    int seen = drawarea()->get_seen_current();
    int num_reserve = drawarea()->get_goto_num_reserve();
    int separator_new = drawarea()->get_separator_new();

    drawarea()->clear_screen();
    drawarea()->set_separator_new( separator_new );
    drawarea()->append_res( 1, get_article()->get_number_load() );
    if( num_reserve ) drawarea()->goto_num( num_reserve );
    else if( seen ) drawarea()->goto_num( seen );
    drawarea()->redraw_view();
}




////////////////////////////////////////////////////////////////////////////////////////////////////

// レス抽出ビュー


ArticleViewRes::ArticleViewRes( const std::string& url,
                                const std::string& num, bool show_title, const std::string& center  )
    : ArticleViewBase( url ),
      m_str_num( num ),
      m_str_center( center ),
      m_show_title( show_title )
{
    struct timeval tv;
    struct timezone tz;
    gettimeofday( &tv, &tz );

    // viewのURL更新
    std::string url_tmp = url_article() + ARTICLE_SIGN + RES_SIGN + m_str_num + CENTER_SIGN;
    if( !m_str_center.empty() ) url_tmp += m_str_center;
    else url_tmp += "0";
    url_tmp += TIME_SIGN + MISC::timevaltostr( tv );
    set_url( url_tmp );

#ifdef _DEBUG
    std::cout << "ArticleViewRes::ArticleViewRes " << get_url() << std::endl;
#endif

    setup_view();
}



ArticleViewRes::~ArticleViewRes()
{

#ifdef _DEBUG    
    std::cout << "ArticleViewRes::~ArticleViewRes : " << get_url() << std::endl;
#endif
}


//
// 抽出表示
//
void ArticleViewRes::show_view()
{
    show_res( m_str_num, m_show_title );

    // ラベルとタブ
    if( get_articletoolbar() ){

        get_articletoolbar()->m_button_board.set_label( "[ " + DBTREE::board_name( url_article() ) + " ]" );
        get_articletoolbar()->set_label( " [ RES:" + m_str_num + " ] - " + DBTREE::article_subject( url_article() ) );
        std::string str_label = "[RES] " + DBTREE::article_subject( url_article() );
        ARTICLE::get_admin()->set_command( "set_tablabel", get_url(), str_label );
    }
}



//
// 画面を消してレイアウトやりなおし & 再描画
//
void ArticleViewRes::relayout()
{
#ifdef _DEBUG
    std::cout << "ArticleViewRes::relayout\n";
#endif

    drawarea()->clear_screen();
    show_res( m_str_num, m_show_title );
    drawarea()->redraw_view();
}


////////////////////////////////////////////////////////////////////////////////////////////////////

// 名前抽出ビュー


ArticleViewName::ArticleViewName( const std::string& url, const std::string& name )
    : ArticleViewBase( url ),
      m_str_name( name )
{
    struct timeval tv;
    struct timezone tz;
    gettimeofday( &tv, &tz );

    // viewのURL更新
    set_url( url_article() + ARTICLE_SIGN + NAME_SIGN + m_str_name + TIME_SIGN + MISC::timevaltostr( tv ) );

#ifdef _DEBUG
    std::cout << "ArticleViewName::ArticleViewName " << get_url() << std::endl;
#endif

    setup_view();
}



ArticleViewName::~ArticleViewName()
{

#ifdef _DEBUG    
    std::cout << "ArticleViewName::~ArticleViewName : " << get_url() << std::endl;
#endif
}


//
// 抽出表示
//
void ArticleViewName::show_view()
{
    show_name( m_str_name, true );

    // ラベルとタブ
    if( get_articletoolbar() ){

        get_articletoolbar()->m_button_board.set_label( "[ " + DBTREE::board_name( url_article() ) + " ]" );
        get_articletoolbar()->set_label( " [ 名前：" + m_str_name + " ] - "
                                     + DBTREE::article_subject( url_article() ));
        std::string str_label = "[名前] " + DBTREE::article_subject( url_article() );
        ARTICLE::get_admin()->set_command( "set_tablabel", get_url(), str_label );
    }

}



//
// 画面を消してレイアウトやりなおし & 再描画
//
void ArticleViewName::relayout()
{
#ifdef _DEBUG
    std::cout << "ArticleViewName::relayout\n";
#endif

    drawarea()->clear_screen();
    show_name( m_str_name, true );
    drawarea()->redraw_view();
}



////////////////////////////////////////////////////////////////////////////////////////////////////

// ID抽出ビュー


ArticleViewID::ArticleViewID( const std::string& url, const std::string& id )
    : ArticleViewBase( url ),
      m_str_id( id )
{
    struct timeval tv;
    struct timezone tz;
    gettimeofday( &tv, &tz );

    // viewのURL更新
    set_url( url_article() + ARTICLE_SIGN + ID_SIGN + m_str_id + TIME_SIGN + MISC::timevaltostr( tv ) );

#ifdef _DEBUG
    std::cout << "ArticleViewID::ArticleViewID " << get_url() << std::endl;
#endif

    setup_view();
}



ArticleViewID::~ArticleViewID()
{

#ifdef _DEBUG    
    std::cout << "ArticleViewID::~ArticleViewID : " << get_url() << std::endl;
#endif
}


//
// 抽出表示
//
void ArticleViewID::show_view()
{
    show_id( m_str_id, true );

    // ラベルとタブ
    if( get_articletoolbar() ){

        get_articletoolbar()->m_button_board.set_label( "[ " + DBTREE::board_name( url_article() ) + " ]" );
        get_articletoolbar()->set_label( " [ ID:" + m_str_id.substr( strlen( PROTO_ID ) ) + " ] - "
                                     + DBTREE::article_subject( url_article() ));
        std::string str_label = "[ID] " + DBTREE::article_subject( url_article() );
        ARTICLE::get_admin()->set_command( "set_tablabel", get_url(), str_label );
    }
}



//
// 画面を消してレイアウトやりなおし & 再描画
//
void ArticleViewID::relayout()
{
#ifdef _DEBUG
    std::cout << "ArticleViewID::relayout\n";
#endif

    drawarea()->clear_screen();
    show_id( m_str_id, true );
    drawarea()->redraw_view();
}



////////////////////////////////////////////////////////////////////////////////////////////////////

// ブックマーク抽出ビュー


ArticleViewBM::ArticleViewBM( const std::string& url )
    : ArticleViewBase( url )
{
    struct timeval tv;
    struct timezone tz;
    gettimeofday( &tv, &tz );

    set_url( url_article() + ARTICLE_SIGN + BOOKMK_SIGN + TIME_SIGN + MISC::timevaltostr( tv ) );

#ifdef _DEBUG
    std::cout << "ArticleViewBM::ArticleViewBM " << get_url() << std::endl;
#endif

    setup_view();
}



ArticleViewBM::~ArticleViewBM()
{

#ifdef _DEBUG    
    std::cout << "ArticleViewBM::~ArticleViewBM : " << get_url() << std::endl;
#endif
}



//
// 抽出表示
//
void ArticleViewBM::show_view()
{
    show_bm();

    // ラベルとタブ
    if( get_articletoolbar() ){

        get_articletoolbar()->m_button_board.set_label( "[ " + DBTREE::board_name( url_article() ) + " ]" );
        get_articletoolbar()->set_label( " [ BM ] - " + DBTREE::article_subject( url_article() ));
        std::string str_label = "[BM] " + DBTREE::article_subject( url_article() );
        ARTICLE::get_admin()->set_command( "set_tablabel", get_url(), str_label );
    }
}



//
// 画面を消してレイアウトやりなおし & 再描画
//
void ArticleViewBM::relayout()
{
#ifdef _DEBUG
    std::cout << "ArticleViewBM::relayout\n";
#endif

    drawarea()->clear_screen();
    show_bm();
    drawarea()->redraw_view();
}




////////////////////////////////////////////////////////////////////////////////////////////////////

// URL抽出ビュー

ArticleViewURL::ArticleViewURL( const std::string& url )
    : ArticleViewBase( url )
{
    struct timeval tv;
    struct timezone tz;
    gettimeofday( &tv, &tz );

    // viewのURL更新
    set_url( url_article() + ARTICLE_SIGN + URL_SIGN + TIME_SIGN + MISC::timevaltostr( tv ) );

#ifdef _DEBUG
    std::cout << "ArticleViewURL::ArticleViewURL " << get_url() << std::endl;
#endif

    setup_view();
}



ArticleViewURL::~ArticleViewURL()
{

#ifdef _DEBUG    
    std::cout << "ArticleViewURL::~ArticleViewURL : " << get_url() << std::endl;
#endif
}


//
// 抽出表示
//
void ArticleViewURL::show_view()
{
    show_res_with_url();

    // ラベルとタブ
    if( get_articletoolbar() ){

        get_articletoolbar()->m_button_board.set_label( "[ " + DBTREE::board_name( url_article() ) + " ]" );
        get_articletoolbar()->set_label( " [ URL ] - " + DBTREE::article_subject( url_article() ));
        std::string str_label = "[URL] " + DBTREE::article_subject( url_article() );
        ARTICLE::get_admin()->set_command( "set_tablabel", get_url(), str_label );
    }
}


//
// 画面を消してレイアウトやりなおし & 再描画
//
void ArticleViewURL::relayout()
{
#ifdef _DEBUG
    std::cout << "ArticleViewURL::relayout\n";
#endif

    drawarea()->clear_screen();
    show_res_with_url();
    drawarea()->redraw_view();
}


////////////////////////////////////////////////////////////////////////////////////////////////////

// 参照抽出ビュー


ArticleViewRefer::ArticleViewRefer( const std::string& url, const std::string& num )
    : ArticleViewBase( url ),
      m_str_num( num )
{
    struct timeval tv;
    struct timezone tz;
    gettimeofday( &tv, &tz );

    set_url( url_article() + ARTICLE_SIGN + REFER_SIGN + m_str_num + TIME_SIGN + MISC::timevaltostr( tv ) );

#ifdef _DEBUG
    std::cout << "ArticleViewRefer::ArticleViewRefer " << get_url() << std::endl;
#endif

    setup_view();
}



ArticleViewRefer::~ArticleViewRefer()
{

#ifdef _DEBUG    
    std::cout << "ArticleViewRefer::~ArticleViewRefer : " << get_url() << std::endl;
#endif
}


//
// 抽出表示
//
void ArticleViewRefer::show_view()
{
    show_refer( atol( m_str_num.c_str() ) );

    // ラベルとタブ
    if( get_articletoolbar() ){

        get_articletoolbar()->m_button_board.set_label( "[ " + DBTREE::board_name( url_article() ) + " ]" );
        get_articletoolbar()->set_label( " [ Re:" + m_str_num + " ] - " + DBTREE::article_subject( url_article() ));
        std::string str_label = "[Re] " + DBTREE::article_subject( url_article() );
        ARTICLE::get_admin()->set_command( "set_tablabel", get_url(), str_label );
    }
}


//
// 画面を消してレイアウトやりなおし & 再描画
//
void ArticleViewRefer::relayout()
{
#ifdef _DEBUG
    std::cout << "ArticleViewRefer::relayout\n";
#endif

    drawarea()->clear_screen();
    show_refer( atol( m_str_num.c_str() ) );
    drawarea()->redraw_view();
}




////////////////////////////////////////////////////////////////////////////////////////////////////

// キーワード抽出ビュー


ArticleViewDrawout::ArticleViewDrawout( const std::string& url, const std::string& query, bool mode_or )
    : ArticleViewBase( url ), m_query( query ), m_mode_or( mode_or )
{
    struct timeval tv;
    struct timezone tz;
    gettimeofday( &tv, &tz );

    //viewのURL更新
    std::string url_tmp = url_article() + ARTICLE_SIGN + KEYWORD_SIGN + m_query + ORMODE_SIGN;
    if( mode_or ) url_tmp += "1";
    else url_tmp += "0";
    url_tmp += TIME_SIGN + MISC::timevaltostr( tv );
    set_url( url_tmp );

#ifdef _DEBUG
    std::cout << "ArticleViewDrawout::ArticleViewDrawout " << get_url() << std::endl;
#endif

    setup_view();
}



ArticleViewDrawout::~ArticleViewDrawout()
{

#ifdef _DEBUG    
    std::cout << "ArticleViewDrawout::~ArticleViewDrawout : " << get_url() << std::endl;
#endif
}


//
// 抽出表示
//
void ArticleViewDrawout::show_view()
{
    drawout_keywords( m_query, m_mode_or, true );

    // ラベルとタブ
    if( get_articletoolbar() ){

        get_articletoolbar()->m_button_board.set_label( "[ " + DBTREE::board_name( url_article() ) + " ]" );
        get_articletoolbar()->set_label( DBTREE::article_subject( url_article() ) );

        std::string str_label;
        if( m_mode_or ) str_label = "[OR] " + DBTREE::article_subject( url_article() );
        else str_label = "[AND] " + DBTREE::article_subject( url_article() );
        ARTICLE::get_admin()->set_command( "set_tablabel", get_url(), str_label );

        slot_push_open_search();
        get_articletoolbar()->m_entry_search.set_text( m_query );
    }
}



//
// 画面を消してレイアウトやりなおし & 再描画
//
void ArticleViewDrawout::relayout()
{
#ifdef _DEBUG
    std::cout << "ArticleViewDrawout::relayout\n";
#endif

    drawarea()->clear_screen();
    drawarea()->clear_highlight();
    drawout_keywords( m_query, m_mode_or, true );
    drawarea()->redraw_view();
}



////////////////////////////////////////////////////////////////////////////////////////////////////

// ログやスレタイ検索抽出ビュー

ArticleViewSearch::ArticleViewSearch( const std::string& url_board,
                                      const std::string& query, int searchmode, bool mode_or )
    : ArticleViewBase( url_board ), m_searchtoolbar( NULL )
    , m_url_board( url_board ), m_searchmode( searchmode ), m_mode_or( mode_or ), 
      m_loading( false )
{
    struct timeval tv;
    struct timezone tz;
    gettimeofday( &tv, &tz );

    //viewのURL更新
    std::string url_tmp = m_url_board + BOARD_SIGN + KEYWORD_SIGN + "query" + ORMODE_SIGN;
    if( mode_or ) url_tmp += "1";
    else url_tmp += "0";
    url_tmp += TIME_SIGN + MISC::timevaltostr( tv );
    set_url( url_tmp );

#ifdef _DEBUG
    std::cout << "ArticleViewSearch::ArticleViewSearch " << get_url() << std::endl;
#endif

    setup_view();
    m_searchtoolbar->m_entry_search.set_text( query );

    CORE::get_search_manager()->sig_search_fin().connect(
        sigc::mem_fun( *this, &ArticleViewSearch::slot_search_fin ) );

    if( m_searchmode == SEARCHMODE_TITLE ) ARTICLE::get_admin()->set_command( "set_tablabel", get_url(), "スレタイ検索" );
    else ARTICLE::get_admin()->set_command( "set_tablabel", get_url(), "ログ検索" );

    reload();
}



ArticleViewSearch::~ArticleViewSearch()
{

#ifdef _DEBUG    
    std::cout << "ArticleViewSearch::~ArticleViewSearch : " << get_url() << std::endl;
#endif

    stop();
}


//
// ウィジットのパッキング
//
// ArticleViewBase::pack_widget()をオーパロードして検索ツールバーをパック
//
void ArticleViewSearch::pack_widget()
{
    // ツールバーの設定
    m_searchtoolbar = Gtk::manage( new SearchToolBar() );
    m_searchtoolbar->m_button_close.signal_clicked().connect( sigc::mem_fun(*this, &ArticleViewSearch::close_view ) );
    m_searchtoolbar->m_button_reload.signal_clicked().connect( sigc::mem_fun(*this, &ArticleViewSearch::reload ) );
    m_searchtoolbar->m_button_stop.signal_clicked().connect( sigc::mem_fun(*this, &ArticleViewSearch::stop ) );

    m_searchtoolbar->m_entry_search.signal_activate().connect( sigc::mem_fun( *this, &ArticleViewSearch::slot_active_search ) );
    m_searchtoolbar->m_entry_search.signal_operate().connect( sigc::mem_fun( *this, &ArticleViewSearch::slot_entry_operate ) );

    pack_start( *m_searchtoolbar, Gtk::PACK_SHRINK, 2 );
    pack_start( *drawarea(), Gtk::PACK_EXPAND_WIDGET, 2 );

    show_all_children();
}


//
// フォーカスイン 
//
void ArticleViewSearch::focus_view()
{
    if( m_list_searchdata.empty() && m_searchtoolbar->m_entry_search.get_text().empty() ) m_searchtoolbar->m_entry_search.grab_focus();
    else ArticleViewBase::focus_view();
}


//
// 表示
//
void ArticleViewSearch::show_view()
{
#ifdef _DEBUG
    std::cout << "ArticleViewSearch::show_view()\n";
#endif

    drawarea()->clear_screen();
    drawarea()->clear_highlight();

    if( m_searchmode == SEARCHMODE_ALLLOG ) append_html( "検索対象：キャッシュ内の全ログ<br>" );
    else if( m_searchmode == SEARCHMODE_TITLE ) append_html( "スレタイ検索 : "
                                                             + MISC::get_hostname( CONFIG::get_url_search_title() ) + "<br>" );
    else append_html( "検索対象：" + DBTREE::board_name( m_url_board ) + "<br>" );

    if( CORE::get_search_manager()->get_id() == get_url() ){

        if( CORE::get_search_manager()->is_searching() ){
            append_html( "検索中・・・" );
            m_loading = true;
            ARTICLE::get_admin()->set_command( "toggle_icon", get_url() );
        }
    }
    else if( CORE::get_search_manager()->is_searching() ){
        SKELETON::MsgDiag mdiag( NULL, "他の検索スレッドが実行中です" );
        mdiag.run();
    }
}


//
// 画面を消してレイアウトやりなおし & 再描画
//
void ArticleViewSearch::relayout()
{
#ifdef _DEBUG
    std::cout << "ArticleViewSearch::relayout\n";
#endif

    std::string query = m_searchtoolbar->m_entry_search.get_text();

    drawarea()->clear_screen();
    drawarea()->clear_highlight();

    std::ostringstream comment;

    if( m_searchmode == SEARCHMODE_ALLLOG ) comment <<  "検索対象：キャッシュ内の全ログ<br>";
    else if( m_searchmode == SEARCHMODE_TITLE ) comment << "スレタイ検索 : "
                                                + MISC::get_hostname( CONFIG::get_url_search_title() ) + "<br>";
    else comment <<  "検索対象：" << DBTREE::board_name( m_url_board ) << "<br>";

    comment << query << " " << m_list_searchdata.size() << " 件<br><br>";

    if( ! m_list_searchdata.empty() ){

        std::list< CORE::SEARCHDATA >::iterator it = m_list_searchdata.begin();
        for(; it != m_list_searchdata.end(); ++it ){

            // 板名表示
            if( m_searchmode == SEARCHMODE_ALLLOG || m_searchmode == SEARCHMODE_TITLE )
                comment << "[ <a href=\"" << DBTREE::url_subject( (*it).url_readcgi ) << "\">" << (*it).boardname << "</a> ] ";

            comment << "<a href=\"" << (*it).url_readcgi << "\">" << (*it).subject << "</a>";

            if( (*it).num ) comment << " ( " << (*it).num << " )";

            // queryの抽出表示
            if( m_searchmode == SEARCHMODE_ALLLOG || m_searchmode == SEARCHMODE_LOG )
                comment << "<br><a href=\"" << PROTO_OR << (*it).url_readcgi + KEYWORD_SIGN + query << "\">" << "抽出表示する" << "</a>";

            comment << "<br><br>";
        }
    }

    append_html( comment.str() );
    drawarea()->redraw_view();
}


//
// 検索終了
//
void ArticleViewSearch::slot_search_fin()
{
    if( CORE::get_search_manager()->get_id() != get_url() ) return;

#ifdef _DEBUG
    std::cout << "ArticleViewSearch::slot_search_fin " << get_url() << std::endl;
#endif

    m_list_searchdata =  CORE::get_search_manager()->get_list_data();
    relayout();

    m_loading = false;
    ARTICLE::get_admin()->set_command( "toggle_icon", get_url() );
}


//
// 再検索
//
void ArticleViewSearch::reload()
{
    if( CORE::get_search_manager()->is_searching() ){
        SKELETON::MsgDiag mdiag( NULL, "他の検索スレッドが実行中です" );
        mdiag.run();
    }

    std::string query = m_searchtoolbar->m_entry_search.get_text();

    // 検索が終わると ArticleViewSearch::slot_search_fin() が呼ばれる
    if( ! query.empty() ){

        if( m_searchmode == SEARCHMODE_TITLE ){

            if( ! SESSION::is_online() ){
                SKELETON::MsgDiag mdiag( NULL, "オフラインです" );
                mdiag.run();
                return;
            }

            CORE::get_search_manager()->search_title( get_url(), query );
        }
        else CORE::get_search_manager()->search( get_url(), m_url_board, query, m_mode_or, ( m_searchmode == SEARCHMODE_ALLLOG ) );
        
        show_view();
        focus_view();
    }
}


//
// 検索停止
//
void ArticleViewSearch::stop()
{
    CORE::get_search_manager()->stop();
}



//
// 検索バーにフォーカス移動
//
void ArticleViewSearch::open_searchbar( bool invert )
{
    m_searchtoolbar->m_entry_search.grab_focus();     
}


//
// 検索entryでenterを押した
//
void ArticleViewSearch::slot_active_search()
{
    reload();
}

//
// 検索entryの操作
//
void ArticleViewSearch::slot_entry_operate( int controlid )
{
    if( controlid == CONTROL::Cancel ) focus_view();
    else if( controlid == CONTROL::DrawOutAnd ) reload();
}
