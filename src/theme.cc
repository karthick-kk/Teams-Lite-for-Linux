#include "theme.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <unistd.h>

namespace fs = std::filesystem;

static std::string get_exe_dir() {
    char buf[4096];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len <= 0) return "";
    buf[len] = '\0';
    return fs::path(buf).parent_path().string();
}

static std::vector<std::string> get_theme_dirs() {
    std::string exe_dir = get_exe_dir();
    std::vector<std::string> dirs;
    if (!exe_dir.empty()) {
        dirs.push_back(exe_dir + "/themes");
        dirs.push_back(exe_dir + "/../data/themes");
    }
    dirs.push_back("/usr/lib/tfl/themes");
    dirs.push_back("/usr/share/tfl/themes");
    return dirs;
}

std::string theme_find_path(const std::string& theme_name) {
    if (theme_name.empty() || theme_name == "none") return "";

    std::string filename = theme_name + ".css";
    for (const auto& dir : get_theme_dirs()) {
        std::string path = dir + "/" + filename;
        if (fs::exists(path)) {
            return fs::canonical(path).string();
        }
    }
    return "";
}

std::string theme_load_css(const std::string& theme_name) {
    std::string path = theme_find_path(theme_name);
    if (path.empty()) return "";

    std::ifstream file(path);
    if (!file.is_open()) return "";

    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::vector<std::string> theme_list_available() {
    std::vector<std::string> themes;

    for (const auto& dir : get_theme_dirs()) {
        if (!fs::exists(dir) || !fs::is_directory(dir)) continue;

        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.path().extension() == ".css") {
                std::string name = entry.path().stem().string();
                // Avoid duplicates
                if (std::find(themes.begin(), themes.end(), name) == themes.end()) {
                    themes.push_back(name);
                }
            }
        }
    }

    std::sort(themes.begin(), themes.end());
    return themes;
}

static std::string escape_css_for_js(const std::string& css) {
    std::string escaped;
    for (char c : css) {
        if (c == '\\') escaped += "\\\\";
        else if (c == '\'') escaped += "\\'";
        else if (c == '\n') escaped += "\\n";
        else if (c == '\r') continue;
        else escaped += c;
    }
    return escaped;
}

// Parse CSS variable declarations from theme CSS
// Returns pairs of (variable-name, value)
static std::vector<std::pair<std::string, std::string>> parse_css_vars(const std::string& css) {
    std::vector<std::pair<std::string, std::string>> vars;
    size_t pos = 0;
    while (pos < css.size()) {
        // Find --varName
        auto dash = css.find("--", pos);
        if (dash == std::string::npos) break;

        // Find the colon
        auto colon = css.find(':', dash);
        if (colon == std::string::npos) break;

        std::string name = css.substr(dash, colon - dash);
        // Trim name
        while (!name.empty() && (name.back() == ' ' || name.back() == '\t')) name.pop_back();

        // Find the value (up to ; or })
        auto val_start = colon + 1;
        auto val_end = css.find_first_of(";}", val_start);
        if (val_end == std::string::npos) val_end = css.size();

        std::string value = css.substr(val_start, val_end - val_start);
        // Trim value and remove !important
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(0, 1);
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.pop_back();
        auto imp = value.find("!important");
        if (imp != std::string::npos) {
            value = value.substr(0, imp);
            while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.pop_back();
        }

        if (!name.empty() && !value.empty()) {
            vars.emplace_back(name, value);
        }

        pos = val_end + 1;
    }
    return vars;
}

// Extract non-variable CSS rules (e.g. scrollbar styles)
static std::string extract_non_var_css(const std::string& css) {
    std::string result;
    size_t pos = 0;
    while (pos < css.size()) {
        // Find next rule block that doesn't start with :root
        auto brace = css.find('{', pos);
        if (brace == std::string::npos) break;

        std::string selector = css.substr(pos, brace - pos);
        // Find matching close brace
        auto close = css.find('}', brace);
        if (close == std::string::npos) break;

        // Keep non-:root rules (scrollbar styles, etc.)
        if (selector.find(":root") == std::string::npos) {
            result += css.substr(pos, close - pos + 1) + "\n";
        }

        pos = close + 1;
        // Skip whitespace
        while (pos < css.size() && (css[pos] == '\n' || css[pos] == '\r' || css[pos] == ' ')) pos++;
    }
    return result;
}

std::string theme_get_inject_js(const std::string& css) {
    auto vars = parse_css_vars(css);
    std::string escaped_full = escape_css_for_js(css);

    // Extract brand colors from theme
    std::string brand = "#E95420", brandHover = "#D44B1C", brandPressed = "#B8400F", brandFg = "#F18A5F";
    std::string bubbleBg = "#3B1035";  // default: Yaru Aubergine
    for (const auto& [name, value] : vars) {
        if (name == "--colorBrandBackground") brand = value;
        else if (name == "--colorBrandBackgroundHover") brandHover = value;
        else if (name == "--colorBrandBackgroundPressed") brandPressed = value;
        else if (name == "--colorBrandForeground1") brandFg = value;
        else if (name == "--tflBubbleBackground") bubbleBg = value;
    }

    // Simple approach: inject <style> tag + periodic element color scan
    std::string js =
        // Part 1: inject the CSS file as a <style> element
        "(function(){"
        "var el=document.getElementById('tfl-theme');"
        "if(!el){el=document.createElement('style');el.id='tfl-theme';document.head.appendChild(el);}"
        "el.textContent='" + escaped_full + "';"
        "})();\n"

        // Part 2: element scanner on a timer (separate IIFE so part 1 works even if this errors)
        "(function(){"
        "var map={"
        "'rgb(127, 133, 245)':'" + brandFg + "',"
        "'rgb(117, 121, 235)':'" + brandFg + "',"
        "'rgb(146, 153, 247)':'" + brandFg + "',"
        "'rgb(91, 95, 199)':'" + brand + "',"
        "'rgb(232, 235, 250)':'" + brandFg + "',"
        "'rgb(98, 100, 167)':'" + brand + "'"
        "};"
        // Separate map for backgrounds — message bubbles get a subtler tint
        "var bgMap={"
        "'rgb(127, 133, 245)':'" + brand + "',"
        "'rgb(91, 95, 199)':'" + brand + "',"
        "'rgb(79, 82, 178)':'" + bubbleBg + "',"
        "'rgb(68, 71, 145)':'" + bubbleBg + "',"
        "'rgb(61, 62, 120)':'" + bubbleBg + "',"
        "'rgb(56, 57, 102)':'" + bubbleBg + "',"
        "'rgb(98, 100, 167)':'" + bubbleBg + "',"
        "'rgb(43, 43, 64)':'" + bubbleBg + "'"
        "};"
        "function scan(){"
        "var els=document.querySelectorAll('*');"
        "for(var i=0;i<els.length;i++){"
        "try{"
        "var s=getComputedStyle(els[i]);"
        "if(map[s.color])els[i].style.setProperty('color',map[s.color],'important');"
        "if(bgMap[s.backgroundColor])els[i].style.setProperty('background-color',bgMap[s.backgroundColor],'important');"
        "if(map[s.borderColor])els[i].style.setProperty('border-color',map[s.borderColor],'important');"
        // SVG fill and stroke
        "if(map[s.fill])els[i].style.setProperty('fill',map[s.fill],'important');"
        "if(map[s.stroke])els[i].style.setProperty('stroke',map[s.stroke],'important');"
        // Also check SVG elements via attribute
        "if(els[i].tagName==='path'||els[i].tagName==='circle'||els[i].tagName==='rect'||els[i].tagName==='polygon'||els[i].tagName==='svg'){"
        "var f=els[i].getAttribute('fill');"
        "if(f){"
        "var fl=f.toLowerCase();"
        "if(fl==='#7f85f5'||fl==='#5b5fc7'||fl==='#7579eb'||fl==='#9299f7'||fl==='#444791'||fl==='#4f52b2'||fl==='#6264a7'||fl==='#3d3e78'||fl==='#383966'){"
        "els[i].setAttribute('fill','" + brand + "');"
        "}"
        "}"
        "}"
        "}catch(e){}"
        "}"
        "}"
        "setTimeout(scan,4000);"
        "setInterval(scan,5000);"
        // Part 3: scan CSSOM for :hover rules with brand colors and generate overrides
        "setTimeout(function(){"
        "  var hoverCSS='';"
        "  for(var i=0;i<document.styleSheets.length;i++){"
        "    try{"
        "      var rules=document.styleSheets[i].cssRules;"
        "      if(!rules)continue;"
        "      for(var j=0;j<rules.length;j++){"
        "        var r=rules[j];"
        "        if(!r.selectorText||r.selectorText.indexOf(':hover')===-1)continue;"
        "        var text=r.cssText;"
        "        var changed=false;"
        "        for(var k in map){"
        "          if(text.indexOf(k)!==-1){text=text.split(k).join(map[k]);changed=true;}"
        "        }"
        "        for(var k in bgMap){"
        "          if(text.indexOf(k)!==-1){text=text.split(k).join(bgMap[k]);changed=true;}"
        "        }"
        "        if(changed)hoverCSS+=text+'\\n';"
        "      }"
        "    }catch(e){}"
        "  }"
        "  if(hoverCSS){"
        "    var hel=document.getElementById('tfl-theme-hover');"
        "    if(!hel){hel=document.createElement('style');hel.id='tfl-theme-hover';document.head.appendChild(hel);}"
        "    hel.textContent=hoverCSS;"
        "  }"
        "},6000);"
        "})();";

    return js;
}

std::string theme_get_remove_js() {
    // No need to clean up — ApplyTheme reloads the page
    return "";
}
