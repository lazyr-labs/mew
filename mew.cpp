// menu with input box that calls command
// input box calls command which populates menu
// g++ -o mew -O3 -march=native mew.cpp -lncursesw -ltbb
// TODO:
// * vim editing commands (f/b/w/etc.)
// * preview (press o in select mode)
// * custom config (colors, commands)
// * unicode
// * segfault when resizing in input mode
// * segfault when executing command at startup with unpopulated menu
// * segfault if config file doesn't exist
// * select in f and F mode
#include <execution>
#include <set>
#include <algorithm>
#include <sstream>
#include <vector>
#include <functional>
#include <fstream>
#include <string>
#include <iostream>
#include <cstdlib>
#include <regex>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ncurses.h>
#include <getopt.h>
#include <unistd.h>

#include "re2/re2.h"
#include "re2/stringpiece.h"

#include "lzapi.h"
#include "querydata.h"
#include "query_parser.h"
#include "fuzzy.h"
#include "scores.h"

namespace qparse = qryparser;
namespace qdata = qrydata;

template<typename A, typename B>
using map = std::unordered_map<A, B>;
template<typename A, typename B>
using cmap = const std::unordered_map<A, B>;
template<typename A>
using vec = std::vector<A>;
template<typename A>
using cvec = const std::vector<A>;
template<typename A>
using vec2d = std::vector<std::vector<A>>;
template<typename A>
using cvec2d = const std::vector<std::vector<A>>;
using str = std::string;
using cstr = const std::string;

/**
*/
template<typename T>
auto tostr(const T& t) -> str { return str(t); }

/**
*/
template<typename T>
auto newVecReserve(int r) -> vec<T> {
    auto v = vec<T>();
    v.reserve(r);
    return v;
}

/**
*/
template<typename T>
auto identity(const T& t) -> T { return t; }

/**
*/
template<typename A, typename B, typename F>
auto mapall(A& a, B& b, F f) -> void {
    std::ranges::transform(a, std::back_inserter(b), f);
}

/**
*/
template<typename T, typename A>
auto isin(T& beg, T& end, const A& a) -> bool {
    return std::find(beg, end, a) != end;
}

/**
*/
template<typename T, typename A>
auto isin(T& t, const A& a) -> bool {
    return std::ranges::find(t, a) != std::end(t);
}

/**
*/
template<typename K, typename V>
auto isin(map<K, V>& t, const K& k) -> bool {return t.contains(k);}

/**
*/
template<typename T>
auto concat(T& a, const T& b) -> void {std::ranges::copy(b, std::back_inserter(a));}

/**
*/
template<typename T>
auto concat(T& a, T&& b) -> void {std::ranges::move(b, std::back_inserter(a));}

/**
*/
template<typename T, typename F>
auto forall(T& t, F f) -> void {
    std::for_each(std::begin(t), std::end(t), f);
}

/**
*/
template<typename T, typename F>
auto forall(const T& t, F f) -> void {
    std::for_each(std::cbegin(t), std::cend(t), f);
}

/**
*/
template<typename T, typename F>
auto pforall(const T& t, F f) -> void {
    std::for_each(std::execution::par, std::cbegin(t), std::cend(t), f);
}

/**
*/
template<typename Integer>
auto range(Integer n) -> vec<Integer> {
    auto r = vec<Integer>(n, 0);
    std::iota(std::begin(r), std::end(r), Integer(0));
    return r;
}

/**
*/
template<typename T>
auto len(const T& t) -> auto { return std::size(t); }

/**
*/
auto len(const char* t) -> auto { return strlen(t); }

/**
*/
auto len(cstr* s) -> auto { return len(*s); }

/**
*/
template<typename T, typename A>
auto remove(T& t, A a) -> void { t.erase(a); }

/**
*/
template<typename T, typename A>
auto remove(T& t, A a, A b) -> void { t.erase(a, b); }

/**
*/
template<typename T, typename A>
auto append(T& t, const A&& a) -> void { t.push_back(a); }

/**
*/
template<typename T, typename A>
auto append(T& t, const A& a) -> void { t.push_back(a); }

/**
*/
template<typename T>
auto insert(std::set<T>& t, const T& a) -> void { t.insert(a); }

/**
*/
template<typename T, typename I, typename A>
auto insert(T& t, I iter, const A&& a) -> void { t.insert(iter, a); }

/**
*/
template<typename T, typename I, typename A>
auto insert(T& t, I iter, const A& a) -> void { t.insert(iter, a); }

/**
*/
template<typename T>
auto clear(T& t) -> void { t.clear(); }

/**
*/
template<typename T, typename F>
auto mapall(std::istream& is, T& t, F f) -> void {
    str line;
    while (std::getline(is, line)) {
        append(t, f(std::move(line)));
    }
}

namespace mew {

auto cmd_modes = vec<char>{'F', 'f', 's'};

class Menu;
class CommandLine;
class Mew;

using KeyCommand = std::function<bool (Mew&, Menu&, CommandLine&)>;
auto create_keymap(cmap<int, KeyCommand>& user_keymap, cmap<int, int>& remap, bool parallel) -> map<int, KeyCommand>;

/**
 * Item to show in `Menu`.
 *
 * * text: the string to show.
 * * info: string containing status information.
*/
class Item {
    friend auto get_text(const Item& i) -> cstr*;
    friend auto get_info(const Item& i) -> cstr*;
    friend auto get_filename(const Item& i) -> cstr*;
    friend auto get_lineno(const Item& i) -> long;
    friend auto tostr(const Item& i) -> str;
    friend auto tostrp(const Item& i) -> const str*;
    friend auto set_selected(Item& i, bool is_selected) -> void;

    public:
        Item(cstr& text) : info("  "), text(text), filename(""), lineno(-1) {}

        Item(cstr& text, cstr& filename, long lineno)
            : info("  "), text(text), filename(filename), lineno(lineno) {}

    private:
        str info;
        str text;
        str filename;
        long lineno;
};

/**
*/
auto set_selected(Item& i, bool is_selected) -> void {
    i.info[0] = is_selected ? ' ' : '*';
}

/**
*/
auto get_text(const Item& i) -> cstr* { return &(i.text); }
auto get_info(const Item& i) -> cstr* { return &(i.info); }
auto get_filename(const Item& i) -> cstr* { return &(i.filename); }
auto get_lineno(const Item& i) -> long { return i.lineno; }

/**
*/
auto tostr(const Item& i) -> str {return i.text;}

/**
*/
auto newItem(cstr& text) -> Item { return Item(text); }

/**
*/
auto tostrp(const Item& i) -> const str* {return &(i.text);}

/**
 * Attributes associated with substrings of an `Item`.
 *
 * * beg: index of item at which to start the attributes.
 * * end: index of item at which to stop the attributes.
 * * attrs: ncurses attributes to apply to the interval.
*/
class ItemAttr {
    friend auto translate(const ItemAttr& i, int start) -> std::tuple<int, int>;
    friend auto getend(const ItemAttr& i) -> long unsigned int;
    friend auto getbeg(const ItemAttr& i) -> long unsigned int;
    friend auto getattr(const ItemAttr& i) -> int;

    public:
        ItemAttr(long unsigned int beg, long unsigned int end, int attrs)
            : beg(beg), end(end), attrs(attrs) {}

    private:
        long unsigned int beg;
        long unsigned int end;
        int attrs;
};

auto getend(const ItemAttr& i) -> long unsigned int {return i.end;}
auto getbeg(const ItemAttr& i) -> long unsigned int {return i.beg;}
auto getattr(const ItemAttr& i) -> int {return i.attrs;}

/**
*/
// Translate the bounds based on `start` being
// the origin.
auto translate(const ItemAttr& i, int start) -> std::tuple<int, int> {
    int attr_beg = i.beg - start;
    attr_beg = (attr_beg < 0 ? 0 : attr_beg);
    int attr_end = (i.end - start);
    return {attr_beg, attr_end};
}

using Lines = vec<Item>;
using LineAttrs = vec<vec<ItemAttr>>;
using MenuData = std::tuple<Lines, LineAttrs>;
using LineGetter = std::function<MenuData (cstr&)>;

auto make_interactive_cmd(str cmd) -> KeyCommand;
auto make_populatemenu_cmd(str cmd) -> KeyCommand;
auto find_regex_parallel(cvec<Item>& items, cstr& pattern) -> MenuData;
auto find_regex_files_parallel(cvec<str>& filenames, cstr& pattern) -> MenuData;

/**
*/
auto newItemAttr(long unsigned int idx) -> ItemAttr {
    return ItemAttr(idx, idx + 1, COLOR_PAIR(2));
            //cur_attrs.emplace_back(mew::ItemAttr{aj, aj + 1, A_REVERSE});
}

/**
*/
class MenuHistoryElem {
    friend auto get_data(const MenuHistoryElem& m) -> const MenuData*;
    friend auto get_text(const MenuHistoryElem& m) -> cstr*;

    public:
        MenuHistoryElem(MenuData&& md, cstr& text)
            : menu_data(md), text(text) {}

    private:
        MenuData menu_data;
        str text;
};

/**
*/
auto get_text(const MenuHistoryElem& m) -> cstr* { return &(m.text); }

/**
*/
auto get_data(const MenuHistoryElem& m) -> const MenuData* {
    return &(m.menu_data);
}

/**
 * Class for managing scrolling.
 *
 * This updates cursor position and the data the cursor refers to in
 * a general way that can be used for most things that need to scroll
 * over data that potentially don't fit on the screen.
 *
 * For data that can change size, use `set_data_end(int)`.
*/
class Scroller {

    friend auto current(const Scroller& s) -> std::tuple<int, int, int>;
    friend auto next(Scroller& s) -> std::tuple<int, int, int>;
    friend auto prev(Scroller& s) -> std::tuple<int, int, int>;
    friend auto scrolled(const Scroller& s) -> bool;
    friend auto repos(Scroller& s, int c) -> void;
    friend auto resize(Scroller& s, int data_end) -> void;

    public:
        Scroller() {}

        /**
         * Constructor.
         *
         * This creates an object with the cursor at 0 pointing
         * to the first data element.
         *
         * @param cursor_max: the maximum value that the cursor
         *      can take (usually less than `LINES` or `COLS`).
         * @param data_end: the size of the data being scrolled.
        */
        Scroller(int cursor_max, int data_end) : cursor(0), cursor_max(cursor_max), data_idx(0), data_beg(0), data_end(data_end), scrolled(false) {
        }

        /**
         * Constructor.
         *
         * This creates an object with the cursor at 0 pointing
         * to the `data_idx`th data element.
         *
         * @param cursor_max: the maximum value that the cursor
         *      can take (usually less than `LINES` or `COLS`).
         * @param data_end: the size of the data being scrolled.
         * @param data_idx: index of the data that the cursor
         *      starts at.
        */
        Scroller(int cursor_max, int data_end, int data_idx) : cursor(0), cursor_max(cursor_max), data_idx(data_idx), data_beg(data_idx), data_end(data_end), scrolled(false) {
        }

        ///**
         //* Set position of cursor.
         //*
         //* @param c: new index of cursor.  If this is greater
         //*      than `cursor_max` passed at construction, then
         //*      the cursor is not updated.
        //*/
        //auto set_cursor(int c) -> void {
            //if (c > cursor_max) { return; }
            //int diff = c - cursor;
            //data_idx = std::min(std::max(data_idx + diff, 0), data_end - 1);
            //cursor = std::min(std::max(cursor + diff, 0), cursor_max);
        //}
//
        ///**
         //* Set data size.
         //*
         //* Useful when the size of the data being scrolled is
         //* dynamic.
         //*
         //* @param m: new data size.
        //*/
        //auto set_data_end(int m) -> void {
            //data_end = m;
        //}
//
        ///**
         //* Get scroll positions.
         //*
         //* @return tuple of `(cursor, data_beg, data_idx)`.
         //*      `data_beg` is the index of the first data item
         //*      visible (pointed to when `cursor = 0`).  `data_idx`
         //*      is the index of the data item pointed to by the
         //*      cursor.
        //*/
        //auto get_pos() -> std::tuple<int, int, int> {
            //return {cursor, data_beg, data_idx};
        //}
//
        ///**
         //* Scroll to the next data item.
         //*
         //* @return true if the cursor had to scroll past `cursor_max`,
         //*      signaling a potential redraw of the screen.  Otherwise
         //*      false.
        //*/
        //auto next() -> bool {
            //data_idx = std::min(data_end - 1, data_idx + 1);
            //cursor += 1;
            //if (cursor >= cursor_max) {
                //data_beg = std::min(data_end - cursor_max, data_beg + 1);
                //cursor = cursor_max - 1;
                //if (data_idx == (data_end - 1)) {
                    //cursor = data_idx - data_beg;
                //}
                //return true;
            //}
            //if (data_idx == (data_end - 1)) {
                //cursor = data_idx - data_beg;
            //}
            //return false;
        //}
//
        ///**
         //* Scroll to the previous data item.
         //*
         //* @return true if the cursor had to scroll before 0,
         //*      signaling a potential redraw of the screen.
         //*      Otherwise false.
        //*/
        //auto prev() -> bool {
            //data_idx = std::max(0, data_idx - 1);
            //if (cursor <= 0) {
                //data_beg = data_idx;
                //cursor = 0;
                //return true;
            //}
            //cursor -= 1;
            //return false;
        //}

    private:
        int cursor;
        int cursor_max;
        int data_idx;
        int data_beg;
        int data_end;
        bool scrolled;
};

auto resize(Scroller& s, int data_end) -> void { s.data_end = data_end; }

auto current(const Scroller& s) -> std::tuple<int, int, int> {
    return {s.cursor, s.data_beg, s.data_idx};
}

auto scrolled(const Scroller& s) -> bool { return s.scrolled; }

/**
 * Set position of cursor.
 *
 * @param c: new index of cursor.  If this is greater
 *      than `cursor_max` passed at construction, then
 *      the cursor is not updated.
 */
auto repos(Scroller& s, int c) -> void {
    if (c > s.cursor_max) return;
    int diff = c - s.cursor;
    s.data_idx = std::min(std::max(s.data_idx + diff, 0), s.data_end - 1);
    s.cursor = std::min(std::max(s.cursor + diff, 0), s.cursor_max);
}

/**
*/
auto next(Scroller& s) -> std::tuple<int, int, int> {
    s.data_idx = std::min(s.data_end - 1, s.data_idx + 1);
    s.cursor += 1;
    if (s.cursor >= s.cursor_max) {
        s.data_beg = std::min(s.data_end - s.cursor_max, s.data_beg + 1);
        s.cursor = s.cursor_max - 1;
        if (s.data_idx == (s.data_end - 1)) {
            s.cursor = s.data_idx - s.data_beg;
        }
        s.scrolled = true;
        return current(s);
    }
    else if (s.data_idx == (s.data_end - 1)) {
        s.cursor = s.data_idx - s.data_beg;
    }
    s.scrolled = false;
    return current(s);
}

auto prev(Scroller& s) -> std::tuple<int, int, int> {
    s.data_idx = std::max(0, s.data_idx - 1);
    if (s.cursor <= 0) {
        s.data_beg = s.data_idx;
        s.cursor = 0;
        s.scrolled = true;
        return current(s);
    }
    s.cursor -= 1;
    s.scrolled = false;
    return current(s);
}

/**
 * A scrollable menu.
 *
 * Items to show are set with `set_items`.  This takes strings
 * and ncurses attributes to show for regions of those strings
 * (eg. color, bold, etc.).
*/
class Menu {
    friend auto prev(Menu& m) -> void;
    friend auto next(Menu& m) -> void;
    friend auto current(const Menu& m) -> str;
    friend auto getall(const Menu& m) -> cvec<Item>*;
    friend auto resize(Menu& m, const std::tuple<int, int, int>& bounds) -> void;
    friend auto setall(Menu& m, cvec<Item>& items, cvec2d<ItemAttr>& attrs) -> void;
    friend auto toggle_selection(Menu& m) -> void;
    friend auto toggle_selection(Menu& m, int line) -> void;
    friend auto toggle_info(Menu& m) -> void;
    friend auto get_selections(const Menu& m) -> vec<str>;

    public:
        Menu() {}

        /**
         * Constructor.
         *
         * @param window window to draw menu on.
         * @param bounds bounds of the menu given as
         *      `(first row, last row, num of columns)`.
        */
        Menu(WINDOW* window, const std::tuple<int, int, int>& bounds) : first_line(std::get<0>(bounds)), last_line(std::get<1>(bounds)), window(window), items(), item_attrs(), selected_items(), n_lines(last_line - first_line), n_cols(std::get<2>(bounds)), show_info(false) {
            scroller = Scroller(n_lines, 0);
        }

    private:

        /**
         * Highlight row `line`, which points to the `idx`th item.
        */
        auto highlight(int line, int idx) -> void {
            wattron(window, COLOR_PAIR(1));
            //wattron(window, A_STANDOUT);
            show_item(idx, line, show_info);
            //wattroff(window, A_STANDOUT);
            wattroff(window, COLOR_PAIR(1));
        }

        /**
         * Unhighlight row `line`, which points to the `idx`th item.
        */
        auto unhighlight(int line, int idx) -> void {
            show_item(idx, line, show_info);
        }

        /**
         * Draw status info for row `line_idx`, which points to the `item_idx`th item.
        */
        auto draw_status(int item_idx, int line_idx) -> void {
            wmove(window, line_idx, 0);
            wclrtoeol(window);
            waddnstr(window, get_info(items[item_idx])->c_str(), len(get_info(items[item_idx])));
        }

        /**
         * Get the index of the `item_idx`th item's string from which
         * to start displaying.  The index is such that the last
         * character of the last attribute is shown in the last
         * column.
        */
        auto get_item_start(int item_idx) -> int {
            if (len(item_attrs) != len(items)) {
                return 0;
            }
            auto n_cols_after_info = n_cols - len(get_info(items[item_idx]));
            auto last_end = getend(item_attrs[item_idx].back());
            if (last_end > n_cols_after_info) {
                return last_end - n_cols_after_info;
                // Uncomment to have the last attribute shown in the
                // third to last column, so that there's more context.
                //return last_end - n_cols_after_info + 3;
            }
            return 0;
        }

        /**
         * Draw attributes of the `item_idx`th item which is currently
         * shown at row `line_idx`.  The string `str` needs to be the
         * item's string offset by `start` (ie, `text.c_str() + start`).
        */
        auto draw_item_attrs(const char* str, int line_idx, int item_idx, int start) -> void {
            if (len(item_attrs) != len(items)) {
                return;
            }

            auto info_len = len(get_info(items[item_idx]));
            for (const auto& attrs : item_attrs[item_idx]) {
                if (getend(attrs) < start) {
                    continue;
                }

                auto [attr_beg, attr_end] = translate(attrs, start);

                wmove(window, line_idx, attr_beg + info_len);
                wattron(window, getattr(attrs));
                waddnstr(window, str + attr_beg, attr_end - attr_beg);
                wattroff(window, getattr(attrs));
            }
        }

        /**
         * Draw the `item_idx`th item which is currently shown at
         * row `line_idx`.
        */
        auto show_item(int item_idx, int line_idx, bool info = false) -> void {
            draw_status(item_idx, line_idx);

            int start = get_item_start(item_idx);
            auto str = get_text(items[item_idx])->c_str() + start;
            auto info_len = len(get_info(items[item_idx]));

            if (info) {
                wmove(window, line_idx, info_len);
                str = get_filename(items[item_idx])->c_str();
                auto lineno = std::to_string(get_lineno(items[item_idx]));
                if (len(*get_filename(items[item_idx])) > (n_cols - info_len - len(lineno) - 1)) {
                    str += len(*get_filename(items[item_idx])) - (n_cols - info_len - len(lineno) - 1);
                }
                waddnstr(window, lineno.c_str(), n_cols - info_len);
                waddnstr(window, " " , n_cols - info_len);
                waddnstr(window, str, n_cols - info_len);
                wmove(window, line_idx, 0);
                return;
            }

            wmove(window, line_idx, info_len);
            waddnstr(window, str, n_cols - info_len);
            draw_item_attrs(str, line_idx, item_idx, start);
            wmove(window, line_idx, 0);
        }

        /**
         * Draw `n_items` items starting from `start_idx`.  The
         * cursor is repositioned to the `cursor`th row, which
         * points to the `data_idx`th item.
        */
        auto show_items(int start_idx, int n_items, int cursor, int data_idx, bool info = false) -> void {
            for (int j = start_idx; j < (start_idx + n_items); ++j) {
                show_item(j, j - start_idx, info);
            }
            wmove(window, cursor, 0);
            highlight(cursor, data_idx);
            return;
        }

        int data_beg;
        int first_line;
        int last_line;
        WINDOW* window;
        vec<Item> items;
        vec2d<ItemAttr> item_attrs;
        std::set<int> selected_items;
        int n_lines;
        int n_cols;
        Scroller scroller;
        bool show_info;
};

/**
 * Get all selected items.
 *
 * @return selected items.
 */
auto get_selections(const Menu& m) -> vec<str> {
    auto selections = newVecReserve<str>(len(m.selected_items));
    for (const auto& item_idx : m.selected_items) {
        append(selections, *get_text(m.items[item_idx]));
    }
    return selections;
}

/**
*/
auto toggle_info(Menu& m) -> void {
    m.show_info = not m.show_info;
    auto [c, db, di] = current(m.scroller);
    m.show_items(db, std::min((int)len(m.items) - db, m.n_lines), c, di, m.show_info);
}

/**
 * Select the current line.
 */
auto toggle_selection(Menu& m) -> void {
    if (std::empty(m.items)) {
        return;
    }

    auto [c, db, di] = current(m.scroller);
    if (isin(m.selected_items, di)) {
        remove(m.selected_items, di);
        set_selected(m.items[di], false);
    }
    else {
        insert(m.selected_items, di);
        set_selected(m.items[di], true);
    }
    m.show_item(di, c, m.show_info);
    m.highlight(c, di);
}

/**
 * Select a line.
 *
 * @param line the line to select.
 */
auto toggle_selection(Menu& m, int line) -> void {
    if (std::empty(m.items)) {
        return;
    }
    auto [c, db, di] = current(m.scroller);
    m.unhighlight(c, di);
    repos(m.scroller, line);
    toggle_selection(m);
}

/**
 * Set and display items in the menu.
 *
 * This shows strings and their attributes in the menu.
 * Each string is associated with a list of attributes
 * (`ItemAttr`s) that are applied to different substrings
 * of the string.
 *
 * @param results strings to show as menu items.
 * @param attr per-string ncurses attributes to apply
 *      to regions of each string.
 */
auto setall(Menu& m, cvec<Item>& items, cvec2d<ItemAttr>& attrs) -> void {
    if (std::empty(items)) {
        return;
    }

    clear(m.items);
    clear(m.item_attrs);
    mapall(items, m.items, identity<Item>);
    if (len(attrs) == len(items)) {
        mapall(attrs, m.item_attrs, identity<vec<ItemAttr>>);
    }

    auto items_len = len(items);
    m.n_lines = std::min(m.last_line - m.first_line, (int)items_len);
    wclear(m.window);
    m.show_items(0, std::min((int)items_len, m.n_lines), 0, 0, m.show_info);
    m.scroller = Scroller(m.n_lines, items_len);
}

/**
 * Resize menu.
 *
 * @param bounds new bounds in which the menu is contained,
 *      given as `(first row, last row, num of columns)`.
 */
auto resize(Menu& m, const std::tuple<int, int, int>& bounds) -> void {
    m.first_line = std::get<0>(bounds);
    m.last_line = std::get<1>(bounds);
    m.n_cols = std::get<2>(bounds);

    auto items_len = len(m.items);
    m.n_lines = std::min(m.last_line - m.first_line, (int)items_len);
    auto [c, db, di] = current(m.scroller);
    m.show_items(db, std::min((int)items_len - db, m.n_lines), 0, db, m.show_info);
    m.scroller = Scroller(m.n_lines, items_len, db);
}

/**
*/
auto getall(const Menu& m) -> cvec<Item>* { return &m.items; }

/**
 * Scroll to the next item.
 */
auto next(Menu& m) -> void {
    if (std::empty(m.items)) {
        return;
    }
    if (auto [c, db, di] = next(m.scroller); scrolled(m.scroller)) {
        m.show_items(db, std::min((int)len(m.items), m.n_lines), c, di, m.show_info);
    }
    else {
        m.unhighlight(c - 1, di - 1);
        m.highlight(c, di);
    }
}

/**
 * Get all selected items.
 *
 * @return selected items.
 */
auto current(const Menu& m) -> str {
    auto [c, db, di] = current(m.scroller);
    return *get_text(m.items[di]);
}

/**
*/
auto prev(Menu& m) -> void {
    if (std::empty(m.items)) {
        return;
    }
    if (auto [c, db, di] = prev(m.scroller); scrolled(m.scroller)) {
        m.show_items(di, std::min((int)len(m.items) - di, m.n_lines), c, di, m.show_info);
    }
    else {
        m.unhighlight(c + 1, di + 1);
        m.highlight(c, di);
    }
}


/**
 * A scrollable text input line.
*/
class CommandLine {
    friend auto clear(CommandLine& c) -> void;
    friend auto prev(CommandLine& cl) -> void;
    friend auto next(CommandLine& cl) -> void;
    friend auto erase(CommandLine& cl) -> void;
    friend auto insert(CommandLine& cl, char c) -> void;
    friend auto resize(CommandLine& c, const std::tuple<int, int>& bounds) -> void;
    friend auto get_text(const CommandLine& c) -> str;
    friend auto set_text(CommandLine& c, cstr& text) -> void;
    friend auto get_mode(const CommandLine& c) -> char;
    friend auto set_mode(CommandLine& c, char ch) -> void;

    public:

        /**
        */
        CommandLine() {}

        /**
         * Constructor.
         *
         * @param window window to draw menu on.
         * @param bounds bounds of the menu given as
         *      `(row, num of columns)`.
        */
        CommandLine(WINDOW* window, const std::tuple<int, int>& bounds) : window(window), text(), status_info("[ ]:"), row(std::get<0>(bounds)), n_cols(std::get<1>(bounds)) {
            scroller = Scroller(n_cols - len(status_info), 0);
        }


    private:

        /**
         * Redraw command line contents.
        */
        auto redraw() -> void {
            auto [c, db, di] = current(scroller);
            auto status_len = len(status_info);
            auto text_len = status_len > n_cols ? 0 : n_cols - status_len;
            wmove(window, row, 0);
            wclrtoeol(window);
            mvwaddnstr(window, row, 0, status_info.c_str(), n_cols);
            mvwaddnstr(window, row, status_len, text.c_str() + db, text_len);
            wmove(window, row, std::min(c + (int)status_len, n_cols));

            //wmove(window, row - 1, 0);
            //wclrtoeol(window);
            //mvwaddnstr(window, row - 1, 0, (text).c_str() + db, n_cols);
        }

        WINDOW *window;
        str text;
        str status_info;
        int row;
        int n_cols;
        Scroller scroller;
};

/**
 * Get the mode currently being shown.
 *
 * @return the mode currently being shown.
 */
auto get_mode(const CommandLine& c) -> char { return c.status_info[1]; }

/**
 * Set the mode to show.
 *
 * @param c the mode to show in the command line.
 */
auto set_mode(CommandLine& c, char ch) -> void {
    c.status_info[1] = ch;
    c.redraw();
}

/**
 * Return the command line text.
 *
 * @return current text.
 */
auto get_text(const CommandLine& c) -> str { return c.text; }

/**
 * Return the command line text.
 *
 * @return current text.
 */
auto set_text(CommandLine& c, cstr& text) -> void {
    c.text = text;
    c.scroller = Scroller(c.n_cols - len(c.status_info), len(c.text) + 1);
    c.redraw();
}

/**
 * Resize command line.
 *
 * @param bounds new bounds in which the menu is contained,
 *      given as `(row, num of columns)`.
 */
auto resize(CommandLine& c, const std::tuple<int, int>& bounds) -> void {
    c.row = std::get<0>(bounds);
    c.n_cols = std::get<1>(bounds);
    auto [cu, db, di] = current(c.scroller);
    c.scroller = Scroller(c.n_cols - len(c.status_info), len(c.text) + 1, db);
    c.redraw();
}

/**
*/
auto clear(CommandLine& c) -> void {
    ::clear(c.text);
    c.scroller = Scroller(c.n_cols - len(c.status_info), 0);
    c.redraw();
}

/**
 * Set the cursor to the next character.
 */
auto next(CommandLine& cl) -> void {
    if (std::empty(cl.text)) {
        return;
    }
    if (auto [c, db, di] = next(cl.scroller); scrolled(cl.scroller)) {
        cl.redraw();
    }
    else {
        wmove(cl.window, cl.row, std::min(c + (int)len(cl.status_info), cl.n_cols));
    }
}

/**
 * Set the cursor to the previous character.
 */
auto prev(CommandLine& cl) -> void {
    if (std::empty(cl.text)) {
        return;
    }
    if (auto [c, db, di] = prev(cl.scroller); scrolled(cl.scroller)) {
        cl.redraw();
    }
    else {
        wmove(cl.window, cl.row, std::min(c + (int)len(cl.status_info), cl.n_cols));
    }
}

/**
 * Remove the character before the cursor.
 */
auto erase(CommandLine& cl) -> void {
    auto [c, db, di] = current(cl.scroller);
    if (std::empty(cl.text) or (di == 0)) {
        return;
    }
    remove(cl.text, std::begin(cl.text) + di - 1);
    resize(cl.scroller, len(cl.text) + 1);
    cl.redraw();
    prev(cl);
}

/**
 * Insert a character before the cursor.
 *
 * @param c character to insert.
 */
auto insert(CommandLine& cl, char c) -> void {
    auto [cu, db, di] = current(cl.scroller);
    if (cu >= (len(cl.text) - db)) {
        cl.text += c;
    }
    else {
        cl.text.insert(di, 1, c);
        //insert(text, std::begin(text) + di, c);
    }
    resize(cl.scroller, len(cl.text) + 1);
    cl.redraw();
    next(cl);
}

/**
*/
template<typename T>
class History {
    template<typename R>
    friend auto next(History<R>& h) -> const R*;
    template<typename R>
    friend auto prev(History<R>& h) -> const R*;
    template<typename R>
    friend auto insert(History<R>& h, R present) -> void;
    template<typename R>
    friend auto add_go_next(History<R>& h, R present) -> void;
    template<typename R>
    friend auto getall(const History<R>& h) -> cvec<R>*;

    public:
        /**
        */
        History() {
            history = vec<T>();
            cur_idx = -1;
        }

    private:
        vec<T> history;
        int cur_idx;
};

/**
*/
template<typename T>
auto next(History<T>& h) -> const T* {
    if (std::empty(h.history)) return nullptr;
    h.cur_idx += h.cur_idx < (len(h.history) - 1) ? 1 : 0;
    return &h.history[h.cur_idx];
}

/**
*/
template<typename T>
auto prev(History<T>& h) -> const T* {
    if (std::empty(h.history)) return nullptr;
    h.cur_idx -= h.cur_idx > 0 ? 1 : 0;
    return &h.history[h.cur_idx];
}

/**
*/
template<typename T>
auto insert(History<T>& h, T present) -> void {
    if (not std::empty(h.history)) {
        remove(h.history, std::begin(h.history) + h.cur_idx + 1, std::end(h.history));
    }
    append(h.history, std::move(present));
    ++h.cur_idx;
}

/**
*/
template<typename T>
auto add_go_next(History<T>& h, T present) -> void {
    insert(h, std::move(present));
    next(h);
}

/**
*/
template<typename T>
auto getall(const History<T>& h) -> cvec<T>* { return &h.history; }

/**
 * An interactive menu.
*/
class Mew {
    friend auto next_menu(Mew& m) -> const MenuHistoryElem*;
    friend auto prev_menu(Mew& m) -> const MenuHistoryElem*;
    friend auto insert_menu(Mew& m, MenuHistoryElem&& e) -> void;
    friend auto next_cmd(Mew& m) -> const Item*;
    friend auto prev_cmd(Mew& m) -> const Item*;
    friend auto insert_cmd(Mew& m, Item&& c) -> void;
    friend auto getall_cmd(const Mew& m) -> cvec<Item>*;
    friend auto next_qry(Mew& m) -> const Item*;
    friend auto prev_qry(Mew& m) -> const Item*;
    friend auto insert_qry(Mew& m, Item&& q) -> void;
    friend auto getall_qry(const Mew& m) -> cvec<Item>*;
    friend auto get_initdata(const Mew& m) -> cvec<Item>*;
    friend auto get_initfiles(const Mew& m) -> cvec<str>*;
    friend auto get_selections(Mew& m) -> vec<str>;
    friend auto show(Mew& m, const MenuData* menu_data) -> void;
    friend auto stop(Mew& m) -> void;
    friend auto close(Mew& m) -> void;
    friend auto get_cmdline_bounds(const Mew& m) -> std::tuple<int, int>;
    friend auto get_menu_bounds(const Mew& m) -> std::tuple<int, int, int>;

    public:

        /**
         * Constructor.
         *
         * @param cmd function to execute when pressing `enter`.
         *      This takes the text from the command line as input
         *      and returns a list of strings and attributes.
        */
        Mew(map<int, KeyCommand>&& user_keymap, map<int, int>&& remap, cvec<Item>* global_data,  cvec<str>* global_filenames, int incremental_thresh=500000, int incremental_file=false, bool parallel = false) : selected_strings(), menu(), cmdline(), quit(false) {
            this->user_keymap = user_keymap;
            this->remap = remap;
            this->parallel = parallel;
            this->incremental_thresh = incremental_thresh;
            this->incremental_file = incremental_file;
            this->global_data = global_data;
            this->global_filenames = global_filenames;
        }

    private:

        /**
         * Start ncurses and all the attributes.
        */
        void init_curses() {
            freopen("/dev/tty", "r", stdin);
            initscr();
            start_color();
            cbreak();
            noecho();
            keypad(stdscr, TRUE);
            set_escdelay(0);
            init_pair(1, COLOR_RED, COLOR_BLACK);
            init_pair(2, COLOR_CYAN, COLOR_BLACK);
            // left | wheel up | wheel down.
            mousemask(BUTTON1_CLICKED | BUTTON4_PRESSED | BUTTON5_PRESSED, 0);
            //init_color(3, 777, 0, 777);
            //init_color(4, 0, 777, 777);
            //init_pair(2, 3, 4);
        }

        /**
         * Show content.
        */
        auto init_screen() -> void {
            init_curses();
            menu = Menu(stdscr, get_menu_bounds(*this));
            cmdline = CommandLine(stdscr, get_cmdline_bounds(*this));
            keymap = create_keymap(user_keymap, remap, parallel);
            //mvprintw(LINES - 3, 0, "Use <SPACE> to select or unselect an item.");
            //mvprintw(LINES - 2, 0, "<ENTER> to see presently selected items(F1 to Exit)");
            //post_menu(this->my_menu);
            refresh();
        }

        vec<str> selected_strings;
        LineGetter cmd;
        WINDOW *menu_win;
        Menu menu;
        CommandLine cmdline;
        bool quit;
        map<int, KeyCommand> keymap;
        int incremental_thresh;
        int incremental_file;
        History<MenuHistoryElem> menu_history;
        History<Item> search_history;
        History<Item> cmd_history;
        map<int, KeyCommand> user_keymap;
        map<int, int> remap;
        bool parallel;
        cvec<Item>* global_data;
        cvec<str>* global_filenames;
};

/**
 * Get bounds of the command line.
 *
 * @return bounds as `(row, num of columns)`.
 */
auto get_cmdline_bounds(const Mew& m) -> std::tuple<int, int> {
    return {LINES - 1, COLS - 1};
}

/**
 * Get bounds of the menu.
 *
 * @return bounds as `(first row, last row, num of columns)`.
 */
auto get_menu_bounds(const Mew& m) -> std::tuple<int, int, int> {
    return {0, LINES - 2, COLS - 10};
}

/**
 * Send signal to stop reading input and start shutting down.
 */
auto stop(Mew& m) -> void {
    m.quit = true;
}

/**
 * End ncurses and stop showing contents.
 */
auto close(Mew& m) -> void {
    endwin();
}

/**
 * Get all selected items.
 *
 * @return selected items.
 */
auto get_selections(Mew& m) -> vec<str> {
    return mew::get_selections(m.menu);
}

/**
 * Draw contents on the screen.
 */
auto show(Mew& m, const MenuData* menu_data = nullptr) -> void {
    m.init_screen();
    if (menu_data != nullptr) {
        const auto& [results, attrs] = *menu_data;
        setall(m.menu, results, attrs);
    }

    set_mode(m.cmdline, 'i');
    while (true) {
        int c = wgetch(stdscr);
        bool handled = false;
        if (isin(m.keymap, c)) {
            handled = m.keymap[c](m, m.menu, m.cmdline);
        }
        if (m.quit) {
            break;
        }
        if (not (handled or isin(cmd_modes, get_mode(m.cmdline)))) {
            insert(m.cmdline, c);
            if ((get_mode(m.cmdline) == '/') and (len(*getall(m.menu)) < m.incremental_thresh)) {
                m.keymap[10](m, m.menu, m.cmdline);
            }
            else if ((get_mode(m.cmdline) == '?') and (m.incremental_file)) {
                m.keymap[10](m, m.menu, m.cmdline);
            }
        }
    }

    close(m);
}

/**
*/
auto get_initdata(const Mew& m) -> cvec<Item>* { return m.global_data; }

/**
*/
auto get_initfiles(const Mew& m) -> cvec<str>* { return m.global_filenames; }

/**
*/
auto next_menu(Mew& m) -> const MenuHistoryElem* { return next(m.menu_history); }
auto prev_menu(Mew& m) -> const MenuHistoryElem* { return prev(m.menu_history); }
auto insert_menu(Mew& m, MenuHistoryElem&& e) -> void {
    add_go_next(m.menu_history, e);
}

/**
*/
auto next_cmd(Mew& m) -> const Item* { return next(m.cmd_history); }
auto prev_cmd(Mew& m) -> const Item* { return prev(m.cmd_history); }
auto insert_cmd(Mew& m, Item&& c) -> void {
    add_go_next(m.cmd_history, c);
}
/**
*/
auto getall_cmd(const Mew& m) -> cvec<Item>* { return getall(m.cmd_history); }

/**
*/
auto next_qry(Mew& m) -> const Item* { return next(m.search_history); }
auto prev_qry(Mew& m) -> const Item* { return prev(m.search_history); }
auto insert_qry(Mew& m, Item&& q) -> void {
    add_go_next(m.search_history, q);
}
/**
*/
auto getall_qry(const Mew& m) -> cvec<Item>* { return getall(m.search_history); }

/**
*/
auto fill_batch(vec2d<Item>& strings, std::istream& is, int batch_size, cstr& filename, long offset) -> long {
    auto n_threads = len(strings);
    auto line = str();
    forall(strings, ::clear<vec<Item>>);
    for (int count = 0; count < batch_size; ++count) {
        for (int j = 0; j < n_threads; ++j) {
            if (not std::getline(is, line)) {
                return -1;
            }
            append(strings[j], Item(line, filename, offset));
            ++offset;
        }
    }
    return offset;
}

/**
*/
auto find_fuzzy_files(cvec<str>& filenames, cstr& pattern, bool parallel = false) -> MenuData {
    auto search_args = qdata::SearchArgs{
        .q=pattern,
        .ignore_case=true,
        .smart_case=true,
        .topk=100,
        .filenames=filenames,
        .parallel=parallel,
        .preserve_order=false,
        .batch_size=10000,
        .max_symbol_dist=10,
        .gap_penalty="linear",
        .word_delims=":;,./-_ \t",
        .show_color=false,
    };
    auto scores = lz::search<scores::LinearScorer>(search_args);

    auto file_matches = newVecReserve<Item>(len(scores));
    auto attrs = newVecReserve<vec<ItemAttr>>(len(scores));
    for (const auto& [score, match] : scores) {
        append(file_matches, Item(match.text, match.filename, match.lineno));

        auto cur_attrs = newVecReserve<ItemAttr>(len(score.path));
        mapall(score.path, cur_attrs, mew::newItemAttr);
        append(attrs, std::move(cur_attrs));
    }
    return {file_matches, attrs};
}

/**
*/
auto find_fuzzy(cvec<Item>& items, cstr& pattern, bool parallel = false) -> MenuData {
    auto search_args = qdata::SearchArgs{
        .q=pattern,
        .ignore_case=true,
        .smart_case=true,
        .topk=100,
        .filenames=vec<str>(),
        .parallel=parallel,
        .preserve_order=false,
        .batch_size=10000,
        .max_symbol_dist=10,
        .gap_penalty="linear",
        .word_delims=":;,./-_ \t",
        .show_color=false,
    };
    auto lines = newVecReserve<str>(len(items));
    mapall(items, lines, tostr);
    auto scores = lz::search<scores::LinearScorer>(search_args, &lines);

    auto file_matches = newVecReserve<Item>(1);
    auto attrs = newVecReserve<vec<ItemAttr>>(1);
    for (const auto& [score, match] : scores) {
        append(file_matches, Item(match.text, match.filename, match.lineno));

        auto cur_attrs = newVecReserve<ItemAttr>(len(score.path));
        mapall(score.path, cur_attrs, mew::newItemAttr);
        append(attrs, std::move(cur_attrs));
    }
    return {file_matches, attrs};
}

/**
*/
auto find_regex_files(cvec<str>& filenames, cstr& pattern, bool parallel = false) -> MenuData {
    if (parallel) {
        return find_regex_files_parallel(filenames, pattern);
    }

    auto attrs = mew::LineAttrs();
    auto file_matches = vec<Item>();
    auto re = std::make_unique<re2::RE2>("(" + pattern + ")");
    auto match = re2::StringPiece();
    for (const auto& filename : filenames) {
        auto is = std::ifstream(filename);
        str line = "";
        long lineno = -1;
        while (std::getline(is, line)) {
            ++lineno;
            if (not RE2::PartialMatch(line, *re, &match)) {
                continue;
            }
            long unsigned int beg = match.data() - line.data();
            attrs.push_back({mew::ItemAttr(beg, beg + len(match), COLOR_PAIR(2))});
            //append(attrs, {mew::ItemAttr{beg, beg + len(match), COLOR_PAIR(2)}});
            //attrs.push_back({mew::ItemAttr{beg, beg + len(match), A_REVERSE}});
            file_matches.push_back(Item(line, filename, lineno));
            //append(file_matches, {"", line, filename, lineno});
        }
        is.close();
    }
    return {file_matches, attrs};
}

/**
*/
auto find_regex(cvec<Item>& items, cstr& pattern, bool parallel = false) -> MenuData {
    if (parallel) {
        return find_regex_parallel(items, pattern);
    }

    auto attrs = mew::LineAttrs();
    auto matches = vec<Item>();
    auto re = std::make_unique<re2::RE2>("(" + pattern + ")");
    auto match = re2::StringPiece();
    for (const auto& item : items) {
        const auto line = *tostrp(item);
        if (not RE2::PartialMatch(line, *re, &match)) {
            continue;
        }
        long unsigned int beg = match.data() - line.data();
        append(matches, Item(line, *get_filename(item), get_lineno(item)));
        append(attrs, vec<mew::ItemAttr>{mew::ItemAttr(beg, beg + len(match), COLOR_PAIR(2))});
    }
    return {matches, attrs};
}

/**
*/
template<typename T>
auto fill_batch(vec2d<T>& strings, cvec<T>& items, int batch_size, int offset) -> int {
    auto n_items = len(items);
    auto n_threads = len(strings);
    forall(strings, ::clear<vec<T>>);
    for (int count = 0; count < batch_size; ++count) {
        for (int j = 0; j < n_threads; ++j) {
            if (offset >= n_items) {
                return offset;
            }
            append(strings[j], items[offset]);
            ++offset;
        }
    }
    return offset;
}

/**
*/
auto find_regex_parallel(cvec<Item>& items, cstr& pattern) -> MenuData {
    unsigned int n_threads = std::thread::hardware_concurrency();
    auto results = vec2d<MenuData>(n_threads);
    auto batch = vec2d<Item>(n_threads);
    auto thread_indices = range(n_threads);

    int n_items_read = 0;
    for (bool stop = false; not stop;) {
        n_items_read = fill_batch(batch, items, 10000, n_items_read);
        stop = n_items_read >= len(items);
        pforall(thread_indices, [&](auto k) {
                append(results[k], find_regex(batch[k], pattern));
                });
    }
    auto lines = vec<Item>();
    auto attrs = mew::LineAttrs();
    for (auto& mdv : results) {
        for (auto& [cur_lines, cur_attrs] : mdv) {
            concat(lines, std::move(cur_lines));
            concat(attrs, std::move(cur_attrs));
        }
    }
    return {lines, attrs};
}

/**
*/
auto find_regex_files_parallel(cvec<str>& filenames, cstr& pattern) -> MenuData {
    unsigned int n_threads = std::thread::hardware_concurrency();
    auto results = vec2d<MenuData>(n_threads);
    auto batch = vec2d<Item>(n_threads);
    auto thread_indices = range(n_threads);

    for (const auto& filename : filenames) {
        auto is = std::ifstream(filename);
        for (long n_items_read = 0; n_items_read > -1;) {
            n_items_read = fill_batch(batch, is, 10000, filename, n_items_read);
            pforall(thread_indices, [&](auto k) {
                    append(results[k], find_regex(batch[k], pattern));
                    });
        }
        is.close();
    }
    auto lines = vec<Item>();
    auto attrs = mew::LineAttrs();
    for (auto& mdv : results) {
        for (auto& [cur_lines, cur_attrs] : mdv) {
            concat(lines, std::move(cur_lines));
            concat(attrs, std::move(cur_attrs));
        }
    }
    return {lines, attrs};
}

/**
 * Create keymap for interacting with Mew.
 *
 * Keys are mapped to functions that can interact with Mew.
*/
auto create_keymap(cmap<int, KeyCommand>& user_keymap, cmap<int, int>& remap, bool parallel) -> map<int, KeyCommand> {
    auto keymap = map<int, KeyCommand>();

    keymap[KEY_MOUSE] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        MEVENT mevent;
        if (getmouse(&mevent) != OK) {
            return true;
        }
        if (mevent.bstate & BUTTON5_PRESSED) { // wheel down.
            next(menu);
        }
        else if (mevent.bstate & BUTTON4_PRESSED) { // wheel up.
            prev(menu);
        }
        else if (mevent.bstate & BUTTON1_CLICKED) { // left click.
            if (mevent.y < (LINES - 2)) {
                toggle_selection(menu, mevent.y);
            }
        }
        return true;
    };
    keymap[KEY_RESIZE] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        wclear(stdscr);
        resize(menu, get_menu_bounds(mew));
        resize(cmdline, get_cmdline_bounds(mew));
        return true;
    };
    keymap[27] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        set_mode(cmdline, 's');
        return true;
    };
    keymap['d'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (not isin(cmd_modes, get_mode(cmdline))) return false;
        clear(cmdline);
        return true;
    };
    keymap['i'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (auto mode = get_mode(cmdline); mode == 'i') {
            return false;
        }
        else if (isin(cmd_modes, mode)) {
            //cmdline.clear();
        }
        else {
            return false;
        }
        set_mode(cmdline, 'i');
        return true;
    };
    keymap['j'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (not isin(cmd_modes, get_mode(cmdline))) return false;
        next(menu);
        return true;
    };
    keymap[KEY_DOWN] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        next(menu);
        return true;
    };
    keymap['k'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (not isin(cmd_modes, get_mode(cmdline))) return false;
        prev(menu);
        return true;
    };
    keymap[KEY_UP] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        prev(menu);
        return true;
    };
    keymap['h'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (not isin(cmd_modes, get_mode(cmdline))) return false;
        prev(cmdline);
        return true;
    };
    keymap[KEY_LEFT] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        prev(cmdline);
        return true;
    };
    keymap['l'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (not isin(cmd_modes, get_mode(cmdline))) return false;
        next(cmdline);
        return true;
    };
    keymap[KEY_RIGHT] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        next(cmdline);
        return true;
    };
    keymap[' '] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (not isin(cmd_modes, get_mode(cmdline))) return false;
        toggle_selection(menu);
        return true;
    };
    keymap[KEY_BACKSPACE] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (isin(cmd_modes, get_mode(cmdline))) return false;
        erase(cmdline);
        return true;
    };
    keymap['q'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (not isin(cmd_modes, get_mode(cmdline))) return false;
        stop(mew);
        return true;
    };
    keymap['X'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (get_mode(cmdline) == 'X') return false;
        if (not isin(cmd_modes, get_mode(cmdline))) return false;
        set_mode(cmdline, 'X');
        return true;
    };
    keymap['x'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (get_mode(cmdline) == 'x') return false;
        if (not isin(cmd_modes, get_mode(cmdline))) return false;
        set_mode(cmdline, 'x');
        return true;
    };
    keymap['/'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (not isin(cmd_modes, get_mode(cmdline))) return false;
        set_mode(cmdline, '/');
        return true;
    };
    keymap['?'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (not isin(cmd_modes, get_mode(cmdline))) return false;
        set_mode(cmdline, '?');
        return true;
    };
    // TODO: this is the same as H.
    keymap['L'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (not isin(cmd_modes, get_mode(cmdline))) return false;
        if (auto mh = next_menu(mew); mh != nullptr) {
            const auto& [items, attrs] = *get_data(*mh);
            setall(menu, items, attrs);
            set_text(cmdline, *get_text(*mh));
        }
        return true;
    };
    keymap['H'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (not isin(cmd_modes, get_mode(cmdline))) return false;
        if (auto mh = prev_menu(mew); mh != nullptr) {
            const auto& [items, attrs] = *get_data(*mh);
            setall(menu, items, attrs);
            set_text(cmdline, *get_text(*mh));
        }
        return true;
    };
    keymap['C'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (get_mode(cmdline) != 's') return false;
        toggle_info(menu);
        return true;
    };
    keymap['F'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (get_mode(cmdline) != 's') return false;
        if (auto h = getall_cmd(mew); not std::empty(*h)) {
            setall(menu, *h, {});
        }
        set_mode(cmdline, 'F');
        return true;
    };
    keymap['f'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (get_mode(cmdline) != 's') return false;
        if (auto h = getall_qry(mew); not std::empty(*h)) {
            setall(menu, *h, {});
        }
        set_mode(cmdline, 'f');
        return true;
    };
    keymap[10] = [parallel](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (auto mode = get_mode(cmdline); (mode == '/') or (mode == '?')) {
            MenuData md;
            const auto cmd_text = get_text(cmdline);
            if (auto items = getall(menu); mode == '/') {
                if (cmd_text[0] == '/') {
                    md = find_regex(*items, cmd_text.substr(1), parallel);
                }
                else {
                    md = find_fuzzy(*items, cmd_text, parallel);
                }
            }
            else if (std::empty(*get_initfiles(mew))) {
                if (cmd_text[0] == '/') {
                    md = find_regex(*get_initdata(mew), cmd_text.substr(1), parallel);
                }
                else {
                    md = find_fuzzy(*get_initdata(mew), cmd_text, parallel);
                }
            }
            else {
                if (cmd_text[0] == '/') {
                    md = find_regex_files(*get_initfiles(mew), cmd_text.substr(1), parallel);
                }
                else {
                    md = find_fuzzy_files(*get_initfiles(mew), cmd_text, parallel);
                }
            }
            const auto& [new_items, attrs] = md;

            if (not std::empty(new_items)) {
                setall(menu, new_items, attrs);
                auto menu_hist_elem = MenuHistoryElem(
                    std::make_tuple(std::move(new_items), std::move(attrs)),
                    cmd_text
                );
               insert_menu(mew, std::move(menu_hist_elem));
            }
            insert_qry(mew, Item(mode + cmd_text));
            return true;
        }
        else if (auto mode = get_mode(cmdline); (mode == 'f')) {
            auto text = current(menu);
            set_text(cmdline, str(std::begin(text) + 1, std::end(text)));
            set_mode(cmdline, text[0]);
            //keymap[10](mew, menu, cmdline);
            return true;
        }
        else if (auto mode = get_mode(cmdline); (mode == 'F')) {
            // TODO: this is the same as `c`.
            auto text = current(menu);
            set_text(cmdline, str(std::begin(text) + 1, std::end(text)));
            set_mode(cmdline, text[0]);
            //keymap[10](mew, menu, cmdline);
            return true;
        }
        else if (auto mode = get_mode(cmdline); (mode == 'X')) {
            set_mode(cmdline, 's');
            make_populatemenu_cmd(get_text(cmdline))(mew, menu, cmdline);
            set_mode(cmdline, 'X');
            insert_cmd(mew, Item('X' + get_text(cmdline)));
            return true;
        }
        else if (auto mode = get_mode(cmdline); (mode == 'x')) {
            set_mode(cmdline, 's');
            make_interactive_cmd(get_text(cmdline))(mew, menu, cmdline);
            set_mode(cmdline, 'x');
            insert_cmd(mew, Item('x' + get_text(cmdline)));
            return true;
        }
        return false;
    };

    // Overwrite defaults.
    auto new_keymap = map<int, KeyCommand>();
    for (const auto& [old_key, new_key] : remap) {
        new_keymap[new_key] = keymap[old_key];
    }
    for (const auto& [key, cmd] : new_keymap) {
        keymap[key] = cmd;
    }
    for (const auto& [key, cmd] : user_keymap) {
        keymap[key] = cmd;
    }

    return keymap;
}

/**
*/
template<typename T, typename A>
auto join(T beg, T end, const A& delim) -> A {
    auto s = A();
    for (; beg < (end - 1); ++beg) {
        concat(s, *beg);
        concat(s, delim);
    }
    concat(s, *beg);
    return s;
}

/**
*/
template<typename T, typename A>
auto join(T&& t, const A& delim) -> A {
    return join(std::begin(t), std::end(t), delim);
}

/**
*/
template<typename T, typename A>
auto join(T& t, const A& delim) -> A {
    return join(std::begin(t), std::end(t), delim);
}

/**
*/
auto split(cstr& s, char delim) -> vec<str> {
    auto strings = vec<str>();
    auto cur = s.data();
    auto beg = cur;
    const auto start = cur;
    auto delim_str = str() + delim;
    while (cur) {
        cur = strpbrk(cur, delim_str.c_str());
        if (not cur) break;
        else append(strings, s.substr(beg - start, cur - beg));
        beg = cur = cur + 1;
    }
    if (beg < (s.data() + len(s))) append(strings, s.substr(beg - start));
    return strings;
}

/**
*/
auto replace_unescaped(cstr& line, cvec<str>& srep, cvec<Item>& arep, cstr& hrep) -> str {
    auto we = split(line, '%');
    if (len(we) == 1) {
        return we[0];
    }

    auto srepp = str();
    auto arepp = str();
    auto hrepp = str();
    auto joined_str = we[0];
    auto n = len(we);
    for (int j = 1; j < n; ++j) {
        if (we[j - 1].back() == '%') {
            joined_str += we[j];
        }
        else if (we[j][0] == 'h') {
            if (std::empty(hrepp)) {
                hrepp = " '" + join(split(hrep, '\''),  str("'\\''")) + "' ";
            }
            joined_str += hrepp + we[j].substr(1);
        }
        else if (we[j][0] == 's') {
            if (std::empty(srepp)) {
                for (const auto& selection : srep) {
                    srepp += " '"
                        + join(split(selection, '\''),  str("'\\''"))
                        + "' ";
                }
            }
            joined_str += srepp + we[j].substr(1);
        }
        else if (we[j][0] == 'a') {
            if (std::empty(arepp)) {
                for (const auto& item : arep) {
                    arepp += " '"
                        + join(split(*get_text(item), '\''),  str("'\\''"))
                        + "' ";
                }
            }
            joined_str += arepp + we[j].substr(1);
        }
        else {
            joined_str += we[j];
        }
    }

    return joined_str;
}

/**
*/
auto make_interactive_cmd(str cmd) -> KeyCommand {
    return [=](mew::Mew& mew, mew::Menu& menu, mew::CommandLine& cmdline) {
        if (not isin(cmd_modes, get_mode(cmdline))) {
            return false;
        }
        auto new_cmd = replace_unescaped(cmd, get_selections(menu), *getall(menu), current(menu));
        if (not std::empty(new_cmd)) {
            std::system(new_cmd.c_str());
        }
        else {
            std::system(cmd.c_str());
        }
        redrawwin(stdscr);
        return true;
    };
}

/**
*/
auto make_populatemenu_cmd(str cmd) -> KeyCommand {
    return [=](mew::Mew& mew, mew::Menu& menu, mew::CommandLine& cmdline) {
        if (not isin(cmd_modes, get_mode(cmdline))) {
            return false;
        }

        auto cmd_str = cmd;
        auto new_cmd1 = replace_unescaped(cmd, get_selections(menu), *getall(menu), current(menu));
        auto new_cmd = std::empty(new_cmd1) ? cmd_str.c_str(): new_cmd1.c_str();
        FILE* fd = popen(new_cmd, "r"); if (fd == NULL) {
            return true;
        }

        int n_bytes = 1 << 10;
        char line[n_bytes];
        auto lines = vec<Item>();
        while (fgets(line, n_bytes, fd) != NULL) {
            auto line_len = len(line);
            auto n_chars = line[line_len - 1] == '\n' ? line_len - 1 : line_len;
            append(lines, mew::newItem(str(line, n_chars)));
        }
        int status = pclose(fd);

        if (not std::empty(lines)) {
            setall(menu, lines, {});
            auto menu_hist_elem = MenuHistoryElem(
                std::make_tuple(std::move(lines), mew::LineAttrs()),
                get_text(cmdline)
            );
            insert_menu(mew, std::move(menu_hist_elem));
        }
        return true;
    };
}

} // namespace mew

/**
*/
auto get_filenames_from_stdin() -> vec<str> {
    auto lines = vec<str>();
    mapall(std::cin, lines, identity<str>);
    return lines;
}

/**
*/
auto get_input_from_stdin() -> mew::MenuData {
    auto lines = vec<mew::Item>();
    mapall(std::cin, lines, mew::newItem);
    return {lines, mew::LineAttrs()};
}

/**
*/
auto read_config(cstr& filename) -> std::tuple<map<int, mew::KeyCommand>, map<int, int>> {
    cstr invalid_remap_msg = "Invalid remap (line %d).  Syntax is 'remap x y', where x and y are single letters.\n";
    cstr invalid_icmd_msg = "Invalid icmd (line %d).  Syntax is 'icmd x y', where x is a single letter and y is a string.\n";
    cstr invalid_cmd_msg = "Invalid icmd (line %d).  Syntax is 'cmd x y', where x is a single letter and y is a string.\n";

    if (std::empty(filename)) {
        return {{}, {}};
    }

    auto remap = map<int, int>();
    auto keymap = map<int, mew::KeyCommand>();
    str line;
    int lineno = 0;
    auto is = std::ifstream(filename);
    while (std::getline(is, line)) {
        ++lineno;
        if (line.starts_with("remap ")) {
            auto fields = mew::split(line, ' ');
            if ((len(fields) != 3)
                    or (len(fields[1]) != 1)
                    or (len(fields[2]) != 1))
            {
                printf(invalid_remap_msg.c_str(), lineno);
                exit(1);
            }
            remap[fields[1][0]] = fields[2][0];
        }
        else if (line.starts_with("icmd ")) {
            auto fields = mew::split(line, ' ');
            if ((len(fields) < 3) or (len(fields[1]) != 1)) {
                printf(invalid_icmd_msg.c_str(), lineno);
                exit(1);
            }
            auto cmd = mew::join(std::cbegin(fields) + 2, std::cend(fields), str(" "));
            keymap[fields[1][0]] = mew::make_interactive_cmd(cmd);
        }
        else if (line.starts_with("cmd ")) {
            auto fields = mew::split(line, ' ');
            if ((len(fields) < 3) or (len(fields[1]) != 1)) {
                printf(invalid_cmd_msg.c_str(), lineno);
                exit(1);
            }
            auto cmd = mew::join(std::cbegin(fields) + 2, std::cend(fields), str(" "));
            keymap[fields[1][0]] = mew::make_populatemenu_cmd(cmd);
        }
    }

    is.close();

    return {keymap, remap};
}

/**
*/
struct CmdLineArgs {
    vec<str> filenames;
    int incremental_thresh;
    bool incremental_file;
    bool parallel;
    str config;
    bool stdin_files;
};

auto get_cmdline_args(int argc, char* argv[]) -> CmdLineArgs {
    auto cmdline_args = CmdLineArgs{
        .filenames=vec<str>(),
        .incremental_thresh=500000,
        .incremental_file=false,
        .parallel=false,
        .config="",
        .stdin_files=false,
    };

    const auto shortopts = "fpTt:c:";
    const int STDIN_FILES='f', CONFIG='c', PARALLEL='p', INCREMENTAL_FILE='T', INCREMENTAL_THRESH='t';

    int opt_idx;
    option longopts[] = {
        option{.name="incremental-thresh", .has_arg=required_argument, .flag=0, .val=INCREMENTAL_THRESH},
        option{.name="incremental-file", .has_arg=no_argument, .flag=0, .val=INCREMENTAL_FILE},
        option{.name="parallel", .has_arg=no_argument, .flag=0, .val=PARALLEL},
        option{.name="config", .has_arg=required_argument, .flag=0, .val=CONFIG},
        option{.name="stdin-files", .has_arg=no_argument, .flag=0, .val=STDIN_FILES},
        option{.name=0, .has_arg=0, .flag=0, .val=0},
    };

    while (true) {
        int c = getopt_long(argc, argv, shortopts, longopts, &opt_idx);

        if (c == -1) {
            break;
        }

        switch (c) {
            case INCREMENTAL_FILE:
                cmdline_args.incremental_file = true;
                break;
            case INCREMENTAL_THRESH:
                cmdline_args.incremental_thresh = std::atoi(optarg);
                break;
            case PARALLEL:
                cmdline_args.parallel = true;
                break;
            case CONFIG:
                cmdline_args.config = optarg;
                break;
            case STDIN_FILES:
                cmdline_args.stdin_files = true;
                break;
        }
    }

    if (optind < argc) {
        //mapall(optind, argc, cmdline_args.filenames, tostr<const char*>);
        for (; optind < argc; ++optind) {
            append(cmdline_args.filenames, str(argv[optind]));
        }
    }

    if (cmdline_args.stdin_files) {
        concat(cmdline_args.filenames, get_filenames_from_stdin());
    }

    return cmdline_args;
}

/**
*/
template<typename T>
auto print(const T& t) -> void {
    std::cout << t << std::endl;
}

auto main(int argc, char *argv[]) -> int {
    setlocale(LC_ALL, ""); // utf8 support.

    const auto args = get_cmdline_args(argc, argv);
    auto [keymap, remap] = read_config(args.config);

    auto menu_data = mew::MenuData();
    auto data = vec<mew::Item>();
    if (std::empty(args.filenames)) {
        menu_data = get_input_from_stdin();
        data = std::get<0>(menu_data);
    }

    auto mew = mew::Mew(
            std::move(keymap),
            std::move(remap),
            &data,
            &args.filenames,
            args.incremental_thresh,
            args.incremental_file,
            args.parallel);
    show(mew, std::empty(args.filenames) ? &menu_data : nullptr);
    forall(get_selections(mew), print<str>);

    return 0;
}

//let create_keymap λ: () → (ℤ map (λ: (Mew&, Menu&, CommandLine&) → 𝔹));
//let count: (from ∈ ℤ, to ∈ ℤ, step ∈ ℤ) → void {
    //let n ∈ ℤ = 0;
    //∀ j ∈ from:to:step (
        //(λ: (x ∈ ℤ) → n { x + 1; })(n);
        //n = (λ: (x ∈ ℤ) → ℤ { x + 1; })(n);
        //n = (λ: (x ∈ ℤ) → { x + 1; })(n);
        //n = (λ: (x ∈ ℤ) → x + 1)(n);
        //n = (λ: x ∈ ℤ → x + 1)(n);
        //m: M = x ∈ ℝ map ℝ (x + 1);
    //)
//};
