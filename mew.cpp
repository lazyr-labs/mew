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

///**
//*/
//template<typename... T>
//auto clear(T&... t) -> void {forall({t...}, [&](auto& tj) {tj->clear();});}

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
auto concat(T& a, T& b) -> void {std::ranges::copy(b, std::back_inserter(a));}

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
struct Item {
    str info;
    str text;
    str filename;
    long lineno;
};

/**
*/
auto tostr(const Item& item) -> str {return item.text;}

/**
*/
auto tostrp(const Item& item) -> const str* {return &(item.text);}

/**
 * Attributes associated with substrings of an `Item`.
 *
 * * beg: index of item at which to start the attributes.
 * * end: index of item at which to stop the attributes.
 * * attrs: ncurses attributes to apply to the interval.
*/
struct ItemAttr {
    long unsigned int beg;
    long unsigned int end;
    int attrs;
};
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
    return ItemAttr{idx, idx + 1, COLOR_PAIR(2)};
            //cur_attrs.emplace_back(mew::ItemAttr{aj, aj + 1, A_REVERSE});
}

/**
*/
struct MenuHistoryElem {
    MenuData menu_data;
    str text;
};

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
        Scroller(int cursor_max, int data_end) : cursor(0), cursor_max(cursor_max), data_idx(0), data_beg(0), data_end(data_end) {
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
        Scroller(int cursor_max, int data_end, int data_idx) : cursor(0), cursor_max(cursor_max), data_idx(data_idx), data_beg(data_idx), data_end(data_end) {
        }

        /**
         * Set position of cursor.
         *
         * @param c: new index of cursor.  If this is greater
         *      than `cursor_max` passed at construction, then
         *      the cursor is not updated.
        */
        auto set_cursor(int c) -> void {
            if (c > cursor_max) { return; }
            int diff = c - cursor;
            data_idx = std::min(std::max(data_idx + diff, 0), data_end - 1);
            cursor = std::min(std::max(cursor + diff, 0), cursor_max);
        }

        /**
         * Set data size.
         *
         * Useful when the size of the data being scrolled is
         * dynamic.
         *
         * @param m: new data size.
        */
        auto set_data_end(int m) -> void {
            data_end = m;
        }

        /**
         * Get scroll positions.
         *
         * @return tuple of `(cursor, data_beg, data_idx)`.
         *      `data_beg` is the index of the first data item
         *      visible (pointed to when `cursor = 0`).  `data_idx`
         *      is the index of the data item pointed to by the
         *      cursor.
        */
        auto get_pos() -> std::tuple<int, int, int> {
            return {cursor, data_beg, data_idx};
        }

        /**
         * Scroll to the next data item.
         *
         * @return true if the cursor had to scroll past `cursor_max`,
         *      signaling a potential redraw of the screen.  Otherwise
         *      false.
        */
        auto next() -> bool {
            data_idx = std::min(data_end - 1, data_idx + 1);
            cursor += 1;
            if (cursor >= cursor_max) {
                data_beg = std::min(data_end - cursor_max, data_beg + 1);
                cursor = cursor_max - 1;
                if (data_idx == (data_end - 1)) {
                    cursor = data_idx - data_beg;
                }
                return true;
            }
            if (data_idx == (data_end - 1)) {
                cursor = data_idx - data_beg;
            }
            return false;
        }

        /**
         * Scroll to the previous data item.
         *
         * @return true if the cursor had to scroll before 0,
         *      signaling a potential redraw of the screen.
         *      Otherwise false.
        */
        auto prev() -> bool {
            data_idx = std::max(0, data_idx - 1);
            if (cursor <= 0) {
                data_beg = data_idx;
                cursor = 0;
                return true;
            }
            cursor -= 1;
            return false;
        }

    private:
        int cursor;
        int cursor_max;
        int data_idx;
        int data_beg;
        int data_end;
};

/**
 * A scrollable menu.
 *
 * Items to show are set with `set_items`.  This takes strings
 * and ncurses attributes to show for regions of those strings
 * (eg. color, bold, etc.).
*/
class Menu {

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

        /**
        */
        auto toggle_info() -> void {
            show_info = not show_info;
            auto [c, db, di] = scroller.get_pos();
            show_items(db, std::min((int)len(items) - db, n_lines), c, di, show_info);
        }

        /**
         * Scroll to the previous item.
        */
        auto scroll_up() -> void {
            if (std::empty(items)) {
                return;
            }
            auto scrolled = scroller.prev();
            auto [c, db, di] = scroller.get_pos();
            if (scrolled) {
                show_items(di, std::min((int)len(items) - di, n_lines), c, di, show_info);
            }
            else {
                unhighlight(c + 1, di + 1);
                highlight(c, di);
            }
        }

        /**
         * Scroll to the next item.
        */
        auto scroll_down() -> void {
            if (std::empty(items)) {
                return;
            }
            auto scrolled = scroller.next();
            auto [c, db, di] = scroller.get_pos();
            if (scrolled) {
                show_items(db, std::min((int)len(items), n_lines), c, di, show_info);
            }
            else {
                unhighlight(c - 1, di - 1);
                highlight(c, di);
            }
        }

        /**
         * Select a line.
         *
         * @param line the line to select.
        */
        auto toggle_selection(int line) -> void {
            if (std::empty(items)) {
                return;
            }
            auto [c, db, di] = scroller.get_pos();
            unhighlight(c, di);
            scroller.set_cursor(line);
            toggle_selection();
        }

        /**
         * Select the current line.
        */
        auto toggle_selection() -> void {
            if (std::empty(items)) {
                return;
            }

            auto [c, db, di] = scroller.get_pos();
            if (selected_items.contains(di)) {
                remove(selected_items, di);
                items[di].info[0] = ' ';
            }
            else {
                insert(selected_items, di);
                items[di].info[0] = '*';
            }
            show_item(di, c, show_info);
            highlight(c, di);
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
        auto set_items(cvec<Item>& items, cvec2d<ItemAttr>& attrs) -> void {
            if (std::empty(items)) {
                return;
            }

            clear(this->items);
            clear(this->item_attrs);
            mapall(items, this->items, identity<Item>);
            if (len(attrs) == len(items)) {
                mapall(attrs, item_attrs, identity<vec<ItemAttr>>);
            }

            auto items_len = len(items);
            n_lines = std::min(last_line - first_line, (int)items_len); 
            wclear(window);
            show_items(0, std::min((int)items_len, n_lines), 0, 0, show_info);
            scroller = Scroller(n_lines, items_len);
        }

        /**
        */
        auto get_items() const -> cvec<Item>* {
            return &items;
        }

        /**
         * Get all selected items.
         *
         * @return selected items.
        */
        auto get_highlighted() -> str {
            auto [c, db, di] = scroller.get_pos();
            return items[di].text;
        }

        /**
         * Get all selected items.
         *
         * @return selected items.
        */
        auto get_selections() -> vec<str> {
            auto selections = newVecReserve<str>(len(selected_items));
            for (const auto& item_idx : selected_items) {
                append(selections, items[item_idx].text);
            }
            return selections;
        }

        /**
         * Resize menu.
         *
         * @param bounds new bounds in which the menu is contained,
         *      given as `(first row, last row, num of columns)`.
        */
        auto resize(const std::tuple<int, int, int>& bounds) -> void {
            first_line = std::get<0>(bounds);
            last_line = std::get<1>(bounds);
            n_cols = std::get<2>(bounds);

            auto items_len = len(items);
            n_lines = std::min(last_line - first_line, (int)items_len); 
            auto [c, db, di] = scroller.get_pos();
            show_items(db, std::min((int)items_len - db, n_lines), 0, db, show_info);
            scroller = Scroller(n_lines, items_len, db);
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
            waddnstr(window, items[item_idx].info.c_str(), len(items[item_idx].info));
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
            auto n_cols_after_info = n_cols - len(items[item_idx].info);
            auto last_end = item_attrs[item_idx].back().end;
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

            auto info_len = len(items[item_idx].info);
            for (const auto& attrs : item_attrs[item_idx]) {
                if (attrs.end < start) {
                    continue;
                }

                // Translate the bounds based on `start` being
                // the origin.
                int attr_beg = attrs.beg - start;
                attr_beg = (attr_beg < 0 ? 0 : attr_beg);
                int attr_end = (attrs.end - start);

                wmove(window, line_idx, attr_beg + info_len);
                wattron(window, attrs.attrs);
                waddnstr(window, str + attr_beg, attr_end - attr_beg);
                wattroff(window, attrs.attrs);
            }
        }

        /**
         * Draw the `item_idx`th item which is currently shown at
         * row `line_idx`.
        */
        auto show_item(int item_idx, int line_idx, bool info = false) -> void {
            draw_status(item_idx, line_idx);

            int start = get_item_start(item_idx);
            auto str = items[item_idx].text.c_str() + start;
            auto info_len = len(items[item_idx].info);

            if (info) {
                wmove(window, line_idx, info_len);
                str = items[item_idx].filename.c_str();
                auto lineno = std::to_string(items[item_idx].lineno);
                if (len(items[item_idx].filename) > (n_cols - info_len - len(lineno) - 1)) {
                    str += len(items[item_idx].filename) - (n_cols - info_len - len(lineno) - 1);
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
 * A scrollable text input line.
*/
class CommandLine {

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

        /**
         * Set the cursor to the previous character.
        */
        auto moveto_prev_char() -> void {
            if (std::empty(text)) {
                return;
            }
            if (scroller.prev()) {
                redraw();
            }
            else {
                auto [c, db, di] = scroller.get_pos();
                wmove(window, row, std::min(c + (int)len(status_info), n_cols));
            }
        }

        /**
         * Set the cursor to the next character.
        */
        auto moveto_next_char() -> void {
            if (std::empty(text)) {
                return;
            }
            if (scroller.next()) {
                redraw();
            }
            else {
                auto [c, db, di] = scroller.get_pos();
                wmove(window, row, std::min(c + (int)len(status_info), n_cols));
            }
        }

        /**
         * Return the command line text.
         *
         * @return current text.
        */
        auto get_text() -> str {
            return text;
        }

        /**
         * Return the command line text.
         *
         * @return current text.
        */
        auto set_text(cstr& text) -> str {
            this->text = text;
            scroller = Scroller(n_cols - len(status_info), len(text) + 1);
            redraw();
            return text;
        }

        /**
         * Remove the character before the cursor.
        */
        auto pop() -> void {
            auto [c, db, di] = scroller.get_pos();
            if (std::empty(text) or (di == 0)) {
                return;
            }
            remove(text, std::begin(text) + di - 1);
            scroller.set_data_end(len(text) + 1);
            redraw();
            moveto_prev_char();
        }

        /**
         * Resize command line.
         *
         * @param bounds new bounds in which the menu is contained,
         *      given as `(row, num of columns)`.
        */
        auto resize(const std::tuple<int, int>& bounds) -> void {
            row = std::get<0>(bounds);
            n_cols = std::get<1>(bounds);
            auto [c, db, di] = scroller.get_pos();
            scroller = Scroller(n_cols - len(status_info), len(text) + 1, db);
            redraw();
        }

        /**
         * Insert a character before the cursor.
         *
         * @param c character to insert.
        */
        auto insert(char c) -> void {
            auto [cu, db, di] = scroller.get_pos();
            if (cu >= (len(text) - db)) {
                text += c;
            }
            else {
                text.insert(di, 1, c);
                //insert(text, std::begin(text) + di, c);
            }
            scroller.set_data_end(len(text) + 1);
            redraw();
            moveto_next_char();
        }

        /**
         * Get the mode currently being shown.
         *
         * @return the mode currently being shown.
        */
        auto get_mode() -> char {
            return status_info[1];
        }

        /**
         * Set the mode to show.
         *
         * @param c the mode to show in the command line.
        */
        auto set_mode(char c) -> void {
            status_info[1] = c;
            redraw();
        }

        /**
        */
        auto clear() -> void {
            text.clear();
            scroller = Scroller(n_cols - len(status_info), 0);
            redraw();
        }

    private:

        /**
         * Redraw command line contents.
        */
        auto redraw() -> void {
            auto [c, db, di] = scroller.get_pos();
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
*/
template<typename T>
class History {

    public:
        /**
        */
        History() {
            history = vec<T>();
            cur_idx = -1;
        }

        /**
        */
        auto next() -> const T* {
            if (std::empty(history)) {
                return nullptr;
            }
            if (cur_idx < (len(history) - 1)) {
                cur_idx += 1;
            }
            return &history[cur_idx];
        }

        /**
        */
        auto prev() -> const T* {
            if (std::empty(history)) {
                return nullptr;
            }
            if (cur_idx > 0) {
                cur_idx -= 1;
            }
            return &history[cur_idx];
        }

        /**
        */
        auto add_go_next(T present) -> void {
            add(std::move(present));
            next();
        }

        /**
        */
        auto add(T present) -> void {
            if (not std::empty(history)) {
                remove(history, std::begin(history) + cur_idx + 1, std::end(history));
            }
            append(history, std::move(present));
            ++cur_idx;
        }

        /**
        */
        auto get_all() const -> cvec<T>* {
            return &history;
        }

    private:
        vec<T> history;
        int cur_idx;
};

/**
 * An interactive menu.
*/
class Mew {

    public:
        History<MenuHistoryElem> menu_history;
        History<Item> search_history;
        History<Item> cmd_history;
        vec<Item>* global_data;
        vec<str>* global_filenames;

        /**
         * Get bounds of the command line.
         *
         * @return bounds as `(row, num of columns)`.
        */
        auto get_cmdline_bounds() -> std::tuple<int, int> {
            return {LINES - 1, COLS - 1};
        }

        /**
         * Get bounds of the menu.
         *
         * @return bounds as `(first row, last row, num of columns)`.
        */
        auto get_menu_bounds() -> std::tuple<int, int, int> {
            return {0, LINES - 2, COLS - 10};
        }

        /**
         * Constructor.
         *
         * @param cmd function to execute when pressing `enter`.
         *      This takes the text from the command line as input
         *      and returns a list of strings and attributes.
        */
        Mew(cmap<int, KeyCommand>& user_keymap, cmap<int, int>& remap, int incremental_thresh=500000, int incremental_file=false, bool parallel = false) : selected_strings(), menu(), cmdline(), quit(false) {
            init_curses();
            menu = Menu(stdscr, get_menu_bounds());
            cmdline = CommandLine(stdscr, get_cmdline_bounds());
            keymap = create_keymap(user_keymap, remap, parallel);
            this->incremental_thresh = incremental_thresh;
            this->incremental_file = incremental_file;
        }

        /**
         * Send signal to stop reading input and start shutting down.
        */
        auto stop() -> void {
            quit = true;
        }

        auto show(const MenuData& menu_data) -> void {
            const auto& [results, attrs] = menu_data;
            menu.set_items(results, attrs);
            show();
        }

        /**
         * Draw contents on the screen.
        */
        auto show() -> void {
            init_screen();

            cmdline.set_mode('i');
            while (true) {
                int c = wgetch(stdscr);
                bool handled = false;
                if (isin(keymap, c)) {
                    handled = keymap[c](*this, menu, cmdline);
                }
                if (quit) {
                    break;
                }
                if (not (handled or isin(cmd_modes, cmdline.get_mode()))) {
                    cmdline.insert(c);
                    if ((cmdline.get_mode() == '/') and (len(*menu.get_items()) < incremental_thresh)) {
                        keymap[10](*this, menu, cmdline);
                    }
                    else if ((cmdline.get_mode() == '?') and (incremental_file)) {
                        keymap[10](*this, menu, cmdline);
                    }
                }
            }

            this->close();
        }

        /**
         * Get all selected items.
         *
         * @return selected items.
        */
        auto get_selections() -> vec<str> {
            return menu.get_selections();
        }

        /**
         * End ncurses and stop showing contents.
        */
        auto close() -> void {
            endwin();
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
};

/**
*/
auto fill_batch(vec2d<Item>& strings, std::istream& is, int batch_size, cstr& filename, long offset) -> long {
    auto n_threads = len(strings);
    auto line = str();
    forall(strings, clear<vec<Item>>);
    for (int count = 0; count < batch_size; ++count) {
        for (int j = 0; j < n_threads; ++j) {
            if (not std::getline(is, line)) {
                return -1;
            }
            append(strings[j], Item{"  ", line, filename, offset});
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
        append(file_matches, Item{"  ", match.text, match.filename, match.lineno});

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
        append(file_matches, Item{"  ", match.text, match.filename, match.lineno});

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
            attrs.push_back({mew::ItemAttr{beg, beg + len(match), COLOR_PAIR(2)}});
            //append(attrs, {mew::ItemAttr{beg, beg + len(match), COLOR_PAIR(2)}});
            //attrs.push_back({mew::ItemAttr{beg, beg + len(match), A_REVERSE}});
            file_matches.push_back({"", line, filename, lineno});
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
        matches.emplace_back(std::move(Item{"  ", line, item.filename, item.lineno}));
        //append(matches, std::move(Item{"  ", line, item.filename, item.lineno}));
        attrs.push_back({mew::ItemAttr{beg, beg + len(match), COLOR_PAIR(2)}});
        //append(attrs, {mew::ItemAttr{beg, beg + len(match), COLOR_PAIR(2)}});
        //attrs.push_back({mew::ItemAttr{beg, beg + len(match), A_REVERSE}});
    }
    return {matches, attrs};
}

/**
*/
template<typename T>
auto fill_batch(vec2d<T>& strings, cvec<T>& items, int batch_size, int offset) -> int {
    auto n_items = len(items);
    auto n_threads = len(strings);
    forall(strings, clear<vec<T>>);
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
            menu.scroll_down();
        }
        else if (mevent.bstate & BUTTON4_PRESSED) { // wheel up.
            menu.scroll_up();
        }
        else if (mevent.bstate & BUTTON1_CLICKED) { // left click.
            if (mevent.y < (LINES - 2)) {
                menu.toggle_selection(mevent.y);
            }
        }
        return true;
    };
    keymap[KEY_RESIZE] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        wclear(stdscr);
        menu.resize(mew.get_menu_bounds());
        cmdline.resize(mew.get_cmdline_bounds());
        return true;
    };
    keymap[27] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        cmdline.set_mode('s');
        return true;
    };
    keymap['d'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (not isin(cmd_modes, cmdline.get_mode())) return false;
        cmdline.clear();
        return true;
    };
    keymap['i'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (cmdline.get_mode() == 'i') {
            return false;
        }
        //if (cmdline.get_mode() == 's') {
        if (std::ranges::find(cmd_modes, cmdline.get_mode()) != std::end(cmd_modes)) {
            //cmdline.clear();
        }
        else {
            return false;
        }
        cmdline.set_mode('i');
        return true;
    };
    keymap['j'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (not isin(cmd_modes, cmdline.get_mode())) return false;
        menu.scroll_down();
        return true;
    };
    keymap[KEY_DOWN] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        menu.scroll_down();
        return true;
    };
    keymap['k'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (not isin(cmd_modes, cmdline.get_mode())) return false;
        menu.scroll_up();
        return true;
    };
    keymap[KEY_UP] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        menu.scroll_up();
        return true;
    };
    keymap['h'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (not isin(cmd_modes, cmdline.get_mode())) return false;
        cmdline.moveto_prev_char();
        return true;
    };
    keymap[KEY_LEFT] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        cmdline.moveto_prev_char();
        return true;
    };
    keymap['l'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (not isin(cmd_modes, cmdline.get_mode())) return false;
        cmdline.moveto_next_char();
        return true;
    };
    keymap[KEY_RIGHT] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        cmdline.moveto_next_char();
        return true;
    };
    keymap[' '] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (not isin(cmd_modes, cmdline.get_mode())) return false;
        menu.toggle_selection();
        return true;
    };
    keymap[KEY_BACKSPACE] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (isin(cmd_modes, cmdline.get_mode())) return false;
        cmdline.pop();
        return true;
    };
    keymap['q'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (not isin(cmd_modes, cmdline.get_mode())) return false;
        mew.stop();
        return true;
    };
    keymap['X'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (cmdline.get_mode() == 'X') return false;
        if (not isin(cmd_modes, cmdline.get_mode())) return false;
        cmdline.set_mode('X');
        return true;
    };
    keymap['x'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (cmdline.get_mode() == 'x') return false;
        if (not isin(cmd_modes, cmdline.get_mode())) return false;
        cmdline.set_mode('x');
        return true;
    };
    keymap['/'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (not isin(cmd_modes, cmdline.get_mode())) return false;
        cmdline.set_mode('/');
        return true;
    };
    keymap['?'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (not isin(cmd_modes, cmdline.get_mode())) return false;
        cmdline.set_mode('?');
        return true;
    };
    // TODO: this is the same as H.
    keymap['L'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (not isin(cmd_modes, cmdline.get_mode())) return false;
        if (auto mh = mew.menu_history.next(); mh != nullptr) {
            const auto& [items, attrs] = mh->menu_data;
            menu.set_items(items, attrs);
            cmdline.set_text(mh->text);
        }
        return true;
    };
    keymap['H'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (not isin(cmd_modes, cmdline.get_mode())) return false;
        if (auto mh = mew.menu_history.prev(); mh != nullptr) {
            const auto& [items, attrs] = mh->menu_data;
            menu.set_items(items, attrs);
            cmdline.set_text(mh->text);
        }
        return true;
    };
    keymap['C'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (cmdline.get_mode() != 's') return false;
        menu.toggle_info();
        return true;
    };
    keymap['F'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (cmdline.get_mode() != 's') return false;
        if (auto h = mew.cmd_history.get_all(); not std::empty(*h)) {
            menu.set_items(*h, {});
        }
        cmdline.set_mode('F');
        return true;
    };
    keymap['f'] = [&](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (cmdline.get_mode() != 's') return false;
        if (auto h = mew.search_history.get_all(); not std::empty(*h)) {
            menu.set_items(*h, {});
        }
        cmdline.set_mode('f');
        return true;
    };
    keymap[10] = [parallel](Mew& mew, Menu& menu, CommandLine& cmdline) {
        if (auto mode = cmdline.get_mode(); (mode == '/') or (mode == '?')) {
            MenuData md;
            if (auto items = menu.get_items(); mode == '/') {
                if (cmdline.get_text()[0] == '/') {
                    md = find_regex(*items, cmdline.get_text().substr(1), parallel);
                }
                else {
                    md = find_fuzzy(*items, cmdline.get_text(), parallel);
                }
            }
            else if (std::empty(*(mew.global_filenames))) {
                if (cmdline.get_text()[0] == '/') {
                    md = find_regex(*(mew.global_data), cmdline.get_text().substr(1), parallel);
                }
                else {
                    md = find_fuzzy(*(mew.global_data), cmdline.get_text(), parallel);
                }
            }
            else {
                if (cmdline.get_text()[0] == '/') {
                    md = find_regex_files(*(mew.global_filenames), cmdline.get_text().substr(1), parallel);
                }
                else {
                    md = find_fuzzy_files(*(mew.global_filenames), cmdline.get_text(), parallel);
                }
            }
            const auto& [new_items, attrs] = md;

            if (not std::empty(new_items)) {
                menu.set_items(new_items, attrs);
                auto menu_hist_elem = MenuHistoryElem{
                    .menu_data = std::make_tuple(std::move(new_items), std::move(attrs)),
                        .text = cmdline.get_text(),
                };
                mew.menu_history.add_go_next(std::move(menu_hist_elem));
            }
            mew.search_history.add_go_next(Item{"  ", mode + cmdline.get_text(), "", -1});
            return true;
        }
        else if (auto mode = cmdline.get_mode(); (mode == 'f')) {
            auto text = menu.get_highlighted();
            cmdline.set_text(str(std::begin(text) + 1, std::end(text)));
            cmdline.set_mode(text[0]);
            //keymap[10](mew, menu, cmdline);
            return true;
        }
        else if (auto mode = cmdline.get_mode(); (mode == 'F')) {
            // TODO: this is the same as `c`.
            auto text = menu.get_highlighted();
            cmdline.set_text(str(std::begin(text) + 1, std::end(text)));
            cmdline.set_mode(text[0]);
            //keymap[10](mew, menu, cmdline);
            return true;
        }
        else if (auto mode = cmdline.get_mode(); (mode == 'X')) {
            cmdline.set_mode('s');
            make_populatemenu_cmd(cmdline.get_text())(mew, menu, cmdline);
            cmdline.set_mode('X');
            mew.cmd_history.add(Item{"  ", 'X' + cmdline.get_text(), "", -1});
            return true;
        }
        else if (auto mode = cmdline.get_mode(); (mode == 'x')) {
            cmdline.set_mode('s');
            make_interactive_cmd(cmdline.get_text())(mew, menu, cmdline);
            cmdline.set_mode('x');
            mew.cmd_history.add(Item{"  ", 'x' + cmdline.get_text(), "", -1});
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

auto replace_all(cstr& line, char old, cstr& rep) -> str {
    auto new_cmd = str("");
    auto seq = line.data();
    const auto end = seq + len(line);
    const auto start = seq;
    auto beg = seq;
    auto old_str = str() + old;
    while (seq) {
        seq = strpbrk(seq, old_str.c_str());
        if (not seq) break;
        if (seq != start) {
            new_cmd += line.substr(beg - start, seq - beg);
        }
        new_cmd += rep;
        beg = seq = seq + 1;
    }
    return new_cmd;
}

/**
*/
template<typename T, typename A>
auto join(T beg, T end, const A& delim) -> A {
    auto s = A();
    for (; beg < (end - 1); ++beg) {
        s += *beg + delim;
    }
    s += *beg;
    return s;
}

/**
*/
template<typename T, typename A>
auto join(T&& strings, const A& delim) -> A {
    return join(std::begin(strings), std::end(strings), delim);
}

/**
*/
template<typename T, typename A>
auto join(T& strings, const A& delim) -> A {
    return join(std::begin(strings), std::end(strings), delim);
}

/**
*/
auto split(cstr& s, char delim) -> vec<str> {
    auto strings = vec<str>();
    auto seq = s.data();
    auto beg = seq;
    const auto start = seq;
    auto delim_str = str() + delim;
    while (seq) {
        seq = strpbrk(seq, delim_str.c_str());
        if (not seq) break;
        if (seq == start) strings.push_back("");
        //if (seq == start) append(strings, "");
        else strings.push_back(s.substr(beg - start, seq - beg));
        //else append(strings, s.substr(beg - start, seq - beg));
        beg = seq = seq + 1;
    }
    if (beg < (s.data() + len(s))) {
        strings.push_back(s.substr(beg - start));
        //append(strings, s.substr(beg - start));
    }
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
                hrepp = "'" + join(split(hrep, '\''),  std::string("'\\''")) + "'";
            }
            joined_str += hrepp + we[j].substr(1);
        }
        else if (we[j][0] == 's') {
            if (std::empty(srepp)) {
                for (const auto& selection : srep) {
                    srepp += " '" + join(split(selection, '\''),  std::string("'\\''")) + "'";
                }
            }
            joined_str += srepp + we[j].substr(1);
        }
        else if (we[j][0] == 'a') {
            if (std::empty(arepp)) {
                for (const auto& item : arep) {
                    arepp += " '" + join(split(item.text, '\''),  std::string("'\\''")) + "'";
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
        if (not isin(cmd_modes, cmdline.get_mode())) {
            return false;
        }
        auto new_cmd = replace_unescaped(cmd, menu.get_selections(), *menu.get_items(), menu.get_highlighted());
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
        if (not isin(cmd_modes, cmdline.get_mode())) {
            return false;
        }

        auto cmd_str = cmd;
        auto new_cmd1 = replace_unescaped(cmd, menu.get_selections(), *menu.get_items(), menu.get_highlighted());
        auto new_cmd = std::empty(new_cmd1) ? cmd_str.c_str(): new_cmd1.c_str();
        FILE* fd = popen(new_cmd, "r");
        if (fd == NULL) {
            return true;
        }

        int n_bytes = 1 << 10;
        char line[n_bytes];
        auto lines = vec<Item>();
        while (fgets(line, n_bytes, fd) != NULL) {
            auto line_len = len(line);
            auto n_chars = line[line_len - 1] == '\n' ? line_len - 1 : line_len;
            append(lines, Item{"  ", str(line, n_chars), "", -1});
        }
        int status = pclose(fd);

        auto attrs = mew::LineAttrs();
        menu.set_items(lines, attrs);
        if (not std::empty(lines)) {
            auto menu_hist_elem = MenuHistoryElem{
                .menu_data = std::make_tuple(std::move(lines), std::move(attrs)),
                .text = cmdline.get_text(),
            };
            mew.menu_history.add_go_next(std::move(menu_hist_elem));
        }
        return true;
    };
}



} // namespace mew

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

    return cmdline_args;
}

/**
*/
auto get_filenames_from_stdin() -> vec<str> {
    auto lines = vec<str>();
    str line;
    while (std::getline(std::cin, line)) {
        lines.emplace_back(line);
        //append(lines, std::move(line));
    }
    return lines;
}

/**
*/
auto get_input_from_stdin() -> mew::MenuData {
    auto lines = vec<mew::Item>();
    str line;
    while (std::getline(std::cin, line)) {
        lines.emplace_back(mew::Item{"  ", line, "", -1});
        //append(lines, std::move(mew::Item{"  ", line, "", -1}));
    }
    return std::make_tuple(lines, mew::LineAttrs());
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
            auto cmd = mew::join(std::cbegin(fields) + 2, std::cend(fields), std::string(" "));
            keymap[fields[1][0]] = mew::make_interactive_cmd(cmd);
        }
        else if (line.starts_with("cmd ")) {
            auto fields = mew::split(line, ' ');
            if ((len(fields) < 3) or (len(fields[1]) != 1)) {
                printf(invalid_cmd_msg.c_str(), lineno);
                exit(1);
            }
            auto cmd = mew::join(std::cbegin(fields) + 2, std::cend(fields), std::string(" "));
            keymap[fields[1][0]] = mew::make_populatemenu_cmd(cmd);
        }
    }

    is.close();

    return {keymap, remap};
}

/**
*/
template<typename T>
auto print(const T& t) -> void {
    std::cout << t << std::endl;
}

auto main(int argc, char *argv[]) -> int {
    setlocale(LC_ALL, ""); // utf8 support.

    auto args = get_cmdline_args(argc, argv);
    if (args.stdin_files) {
        auto al = get_filenames_from_stdin();
        forall(al, [&](auto& a) {append(args.filenames, std::move(a));});
        //concat(args.filenames, std::move(al));
    }
    const auto& [keymap, remap] = read_config(args.config);

    if (std::empty(args.filenames)) {
        auto menu_data = get_input_from_stdin();
        auto mew = mew::Mew(keymap, remap, args.incremental_thresh, args.incremental_file, args.parallel);
        mew.global_data = &std::get<0>(menu_data);
        mew.global_filenames = &args.filenames;
        mew.show(menu_data);
        forall(mew.get_selections(), print<str>);
    }
    else {
        auto mew = mew::Mew(keymap, remap, args.incremental_thresh, args.incremental_file, args.parallel);
        auto empty_vec = vec<mew::Item>();
        mew.global_data = &empty_vec;
        mew.global_filenames = &args.filenames;
        mew.show();
        forall(mew.get_selections(), print<str>);
    }

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
