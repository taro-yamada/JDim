// ライセンス: GPL2

// スレ表示部のベースクラス

#ifndef _DRAWAREABASE_H
#define _DRAWAREABASE_H

#include "caret.h"
#include "scrollinfo.h"

#include "jdlib/refptr_lock.h"

#include "colorid.h"
#include "control.h"
#include "cssmanager.h"

#include <gtkmm.h>

namespace ARTICLE
{
    struct LAYOUT;
    class LayoutTree;
    class EmbeddedImage;

    // マウスボタンプレスとリリースのシグナル。リリース時にマウスがリンクの上にある時そのURLを渡す
    typedef sigc::signal< bool, std::string, int, GdkEventButton* > SIG_BUTTON_PRESS;
    typedef sigc::signal< bool, std::string, int, GdkEventButton* > SIG_BUTTON_RELEASE;

    typedef sigc::signal< bool, GdkEventCrossing* > SIG_LEAVE_NOTIFY;
    typedef sigc::signal< bool, GdkEventMotion* > SIG_MOTION_NOTIFY;
    typedef sigc::signal< bool, GdkEventKey* > SIG_KEY_PRESS;
    typedef sigc::signal< bool, GdkEventKey* > SIG_KEY_RELEASE;
    typedef sigc::signal< bool, GdkEventScroll* > SIG_SCROLL_EVENT;

    typedef sigc::signal< void, std::string, int > SIG_ON_URL;
    typedef sigc::signal< void > SIG_LEAVE_URL;
    
    // 範囲選択用
    struct SELECTION
    {
        bool select;
        CARET_POSITION caret_from;
        CARET_POSITION caret_to;
        std::string str;
    };


    ///////////////////////////////////


    class DrawAreaBase : public Gtk::HBox
    {
        SIG_BUTTON_PRESS m_sig_button_press;
        SIG_BUTTON_RELEASE m_sig_button_release;
        SIG_SCROLL_EVENT m_sig_scroll_event;
        SIG_LEAVE_NOTIFY m_sig_leave_notify;
        SIG_MOTION_NOTIFY m_sig_motion_notify;
        SIG_KEY_PRESS m_sig_key_press;
        SIG_KEY_RELEASE m_sig_key_release;
        SIG_ON_URL m_sig_on_url;
        SIG_LEAVE_URL m_sig_leave_url;

        std::string m_url;

        // スピードを稼ぐためデータベースに直接アクセスする
        JDLIB::RefPtr_Lock< DBTREE::ArticleBase > m_article; 

        // HBoxに張り付けるウイジット
        Gtk::DrawingArea m_view;
        Gtk::EventBox* m_event;
        Gtk::VScrollbar* m_vscrbar;

        // レイアウトツリー
        LayoutTree* m_layout_tree;

        // 描画領域の幅、高さ( != ウィンドウサイズ )
        int m_width_client; 
        int m_height_client;

        //現在見ているレスの番号
        int m_seen_current;

        // 描画用
        Glib::RefPtr< Gdk::Window > m_window;
        Glib::RefPtr< Gdk::GC > m_gc;
        Glib::RefPtr< Gdk::Pixmap > m_backscreen;
        Glib::RefPtr< Pango::Layout > m_pango_layout;
        Glib::RefPtr< Pango::Context > m_context;

        // キャレット情報
        CARET_POSITION m_caret_pos;           // 現在のキャレットの位置(クリックやドラッグすると移動)
        CARET_POSITION m_caret_pos_pre;       // 移動前のキャレットの位置(選択範囲の描画などで使用する)
        CARET_POSITION m_caret_pos_dragstart; // ドラッグを開始したキャレット位置

        // 色
        int m_colorid_text;     // デフォルトの文字色
        int m_colorid_back;     // デフォルトの背景色
        std::vector< Gdk::Color > m_color;

        // 枠を描くか
        bool m_draw_frame;

        // 範囲選択
        SELECTION m_selection;

        // 検索結果のハイライト範囲
        std::list< SELECTION > m_multi_selection;
      
        // レイアウト用
        CORE::CSS_PROPERTY m_css_body; // body の cssプロパティ
        int m_fontid;
        int m_font_ascent;
        int m_font_descent;
        int m_font_height;
        int m_underline_pos; // 下線の位置(文字の上からのピクセル値)
        int m_br_size; // 改行量 (ピクセル値)
        int m_mrg_right; // wrap計算のときに使用する右マージン幅 (ピクセル値)

        // ビューのリサイズ用
        bool m_configure_reserve; // true の時は再描画する前に再レイアウトする
        int m_configure_width;
        int m_configure_height;

        // スクロール情報
        SCROLLINFO m_scrollinfo;
        guint32 m_wheel_scroll_time; // 前回ホイールを回した時刻
        int m_goto_num_reserve; // 初期化時のジャンプ予約(レス番号)
        int m_pre_pos_y; // ひとつ前のスクロールバーの位置。スクロールした時の差分量計算に使用する
        std::vector< int > m_jump_history;  // ジャンプ履歴

        // 状態
        int m_x_pointer, m_y_pointer;  // 現在のマウスポインタの位置
        bool m_key_press; // キーを押している
        bool m_drugging;  // ドラッグ中
        bool m_r_drugging; // 右ドラッグ中
        std::string m_link_current; // 現在マウスポインタの下にあるリンクの文字列
        LAYOUT* m_layout_current; // 現在マウスポインタの下にあるlayoutノード(下が空白ならNULL)
        Gdk::CursorType m_cursor_type; // カーソルの形状

        // 入力コントローラ
        CONTROL::Control m_control;

        // 埋め込み画像のポインタ
        std::list< EmbeddedImage* > m_eimgs;

        // ブックマークアイコン
        Glib::RefPtr< Gdk::Pixbuf > m_pixbuf_bkmk;

        // 書き込みマークアイコン
        Glib::RefPtr< Gdk::Pixbuf > m_pixbuf_post;

      public:

        SIG_BUTTON_PRESS sig_button_press(){ return m_sig_button_press; }
        SIG_BUTTON_RELEASE sig_button_release(){ return m_sig_button_release; }
        SIG_SCROLL_EVENT sig_scroll_event(){ return m_sig_scroll_event; }
        SIG_LEAVE_NOTIFY sig_leave_notify(){ return m_sig_leave_notify; }
        SIG_MOTION_NOTIFY sig_motion_notify(){ return m_sig_motion_notify; }
        SIG_KEY_PRESS sig_key_press(){ return m_sig_key_press; }
        SIG_KEY_RELEASE sig_key_release(){ return m_sig_key_release; }
        SIG_ON_URL sig_on_url(){ return m_sig_on_url; }
        SIG_LEAVE_URL sig_leave_url(){ return m_sig_leave_url; }

        DrawAreaBase( const std::string& url );
        ~DrawAreaBase();

        const std::string& get_url() const { return m_url; }
        const int& width_client() const { return m_width_client; }
        const int& height_client() const { return m_height_client; }

        void clock_in();
        void clock_in_smooth_scroll();
        void focus_view();
        void focus_out();

        // フォントID( fontid.h にある ID を指定)
        const int get_fontid() const { return m_fontid; }
        void set_fontid( int id ){ m_fontid = id; }

        // 新着セパレータのあるレス番号の取得とセット
        const int get_separator_new();
        void set_separator_new( int num );
        void hide_separator_new();

        // セパレータが画面に表示されているか
        const bool is_separator_on_screen();

        // 全選択
        void select_all(); 

        // 範囲選択中の文字列
        const std::string str_selection(); 

        // 範囲選択を開始したレス番号
        const int get_selection_resnum_from();

        // 範囲選択を終了したレス番号
        const int get_selection_resnum_to();

        const int get_seen_current() const { return m_seen_current; } // 現在見ているレスの番号
        const int get_goto_num_reserve() const { return m_goto_num_reserve; } // 初期化時のジャンプ予約(レス番号)

        int max_number();   // 表示されている最後のレスの番号

        // レスをappendして再レイアウト
        void append_res( int from_num, int to_num );

        // リストで指定したレスをappendして再レイアウト
        void append_res( std::list< int >& list_resnum );

        // リストで指定したレスをappendして再レイアウト( 連結情報付き )
        // list_joint で連結指定したレスはヘッダを取り除いて前のレスに連結する
        void append_res( std::list< int >& list_resnum, std::list< bool >& list_joint );

        // html をappendして再レイアウト
        void append_html( const std::string& html );

        // datをappendして再レイアウト
        void append_dat( const std::string& dat, int num );

        // 全画面消去
        void clear_screen();

        // バックスクリーンを描き直して再描画予約(queue_draw())する。再レイアウトはしない
        void redraw_view();  

        // スクロール方向指定
        bool set_scroll( const int& control );

        // マウスホイールの処理
        void wheelscroll( GdkEventScroll* event );

        // ジャンプ
        void set_jump_history(); // ジャンプ履歴に現在のスレを登録
        void goto_num( int num );
        void goto_next_res();
        void goto_pre_res();
        void goto_new();
        void goto_top();
        void goto_bottom();
        void goto_back();

        // 検索
        int search( std::list< std::string >& list_query, bool reverse );
        int search_move( bool reverse );
        void clear_highlight();

        // 実況モード
        void live_start();
        void live_stop();
        void update_live_speed( const int sec );

      protected:

        // バックスクリーン描画
        bool draw_backscreen( const bool redraw_all = false );

        // 文字色のID( colorid.h にある ID を指定)
        const int get_colorid_text() const{ return m_colorid_text; }
        void set_colorid_text( int id ){ m_colorid_text = id; }

        // 背景色のID( colorid.h にある ID を指定)
        const int get_colorid_back();
        void set_colorid_back( int id ){ m_colorid_back = id; }

        // 共通セットアップ
        void setup( bool show_abone, bool show_scrbar );

        // レイアウト処理
        virtual bool exec_layout();
        bool exec_layout_impl( const bool init_popupwin, const int offset_y, const int right_mrg );

        // バックスクリーンをDrawAreaにコピー
        bool draw_drawarea( int x = 0, int y = 0, int width = 0, int height = 0 );

        // DrawAreaに枠を描画
        void set_draw_frame( bool draw_frame ){ m_draw_frame = draw_frame; }
        void draw_frame();

        // リサイズした
        virtual bool slot_configure_event( GdkEventConfigure* event );

      private:

        // 初期化関係
        void clear();
        void create_scrbar();
        void init_color();
        void init_font();

        // レイアウト処理
        void set_align( LAYOUT* div, int id_end, int align );
        void set_align_line( LAYOUT* div, LAYOUT* layout_from, LAYOUT* layout_to, RECTANGLE* rect_from, RECTANGLE* rect_to,
                             int width_line, int align );
        void layout_one_text_node( LAYOUT* node, int& node_x, int& node_y, int& br_size, const int width_view );
        void layout_one_img_node( LAYOUT* node, int& node_x, int& node_y, int& brsize, const int width_view, const bool init_popupwin );

        // 文字の幅などの情報
        int get_width_of_one_char( const char* str, int& byte, char& pre_char, bool& wide_mode, const int mode );
        bool set_init_wide_mode( const char* str, const int pos_start, const int pos_to );
        bool is_wrapped( const int x, const int border, const char* str );

        // 描画
        bool draw_one_node( LAYOUT* layout, const int width_win, const int pos_y, const int upper, const int lower );
        void draw_div( LAYOUT* layout_div, const int pos_y, const int upper, const int lower );
        void draw_one_text_node( LAYOUT* layout, const int width_win, const int pos_y );
        void draw_string( LAYOUT* node, const int pos_y, const int width_view,
                          const int color, const int color_back, const int byte_from, const int byte_to );
        bool draw_one_img_node( LAYOUT* layout, const int pos_y, const int upper, const int lower );

        // drawarea がリサイズ実行
        void configure_impl();

        // 整数 -> 文字変換してノードに発言数をセット
        int set_num_id( LAYOUT* layout );

        // スクロール関係
        void exec_scroll( bool redraw_all ); // スクロールやジャンプを実行して再描画
        int get_vscr_val();
        int get_vscr_maxval();

        // キャレット関係
        bool is_pointer_on_rect( const RECTANGLE* rect, const char* text, const int pos_start, const int pos_to,
                                 const int x, const int y,
                                 int& pos, int& width_line, int& char_width, int& byte_char );
        LAYOUT* set_caret( CARET_POSITION& caret_pos, int x, int y );
        const bool set_carets_dclick( CARET_POSITION& caret_left, CARET_POSITION& caret_right,  const int x, const int y, const bool triple );

        // 範囲選択関係
        bool set_selection( CARET_POSITION& caret_left, CARET_POSITION& caret_right, const bool redraw = true );
        bool set_selection( CARET_POSITION& caret_pos, const bool redraw = true );
        bool set_selection_str();
        const bool is_caret_on_selection( const CARET_POSITION& caret_pos );
        std::string get_selection_as_url( const CARET_POSITION& caret_pos );

        // マウスが動いた時の処理
        bool motion_mouse();

        // 現在のポインターの下のノードからカーソルのタイプを決定する
        const Gdk::CursorType get_cursor_type();

        // カーソルの形状の変更
        void change_cursor( const Gdk::CursorType type );

        // スロット
        void slot_change_adjust();
        bool slot_expose_event( GdkEventExpose* event );
        bool slot_scroll_event( GdkEventScroll* event );
        bool slot_leave_notify_event( GdkEventCrossing* event );
        void slot_realize();

        bool slot_button_press_event( GdkEventButton* event );
        bool slot_button_release_event( GdkEventButton* event );
        bool slot_motion_notify_event( GdkEventMotion* event );

        bool slot_key_press_event( GdkEventKey* event );
        bool slot_key_release_event( GdkEventKey* event );
    };
}

#endif

