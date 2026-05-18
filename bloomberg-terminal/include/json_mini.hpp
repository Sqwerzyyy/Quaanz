#pragma once
// Minimal JSON value parser — no external deps, handles Yahoo Finance responses
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <charconv>
#include <cstring>

struct JsonVal {
    enum Type { Null, Bool, Number, String, Array, Object } type = Null;
    double                          num  = 0;
    bool                            b    = false;
    std::string                     str;
    std::vector<JsonVal>            arr;
    std::map<std::string,JsonVal>   obj;

    bool   is_null()   const { return type == Null;   }
    bool   is_num()    const { return type == Number; }
    bool   is_str()    const { return type == String; }
    bool   is_arr()    const { return type == Array;  }
    bool   is_obj()    const { return type == Object; }
    double as_num()    const { return num; }
    const std::string& as_str() const { return str; }

    const JsonVal& operator[](const std::string& k) const {
        static JsonVal nil;
        auto it = obj.find(k);
        return it != obj.end() ? it->second : nil;
    }
    const JsonVal& operator[](size_t i) const {
        static JsonVal nil;
        return i < arr.size() ? arr[i] : nil;
    }
    size_t size() const { return type==Array ? arr.size() : obj.size(); }
};

class JsonParser {
    const char* p;
    const char* end;

    void skip_ws() { while (p < end && (*p==' '||*p=='\t'||*p=='\n'||*p=='\r')) ++p; }

    std::string parse_string() {
        ++p; // skip "
        std::string s;
        while (p < end && *p != '"') {
            if (*p == '\\') {
                ++p;
                switch(*p) {
                    case '"':  s+='"';  break;
                    case '\\': s+='\\'; break;
                    case '/':  s+='/';  break;
                    case 'n':  s+='\n'; break;
                    case 'r':  s+='\r'; break;
                    case 't':  s+='\t'; break;
                    default:   s+='?';  break;
                }
            } else { s += *p; }
            ++p;
        }
        if (p < end) ++p; // skip closing "
        return s;
    }

    double parse_number() {
        const char* start = p;
        if (*p == '-') ++p;
        while (p < end && ((*p>='0'&&*p<='9')||*p=='.'||*p=='e'||*p=='E'||*p=='+'||*p=='-')) ++p;
        double v = 0;
        // Use strtod for robustness
        char buf[64] = {};
        size_t len = std::min((size_t)(p-start), sizeof(buf)-1);
        memcpy(buf, start, len);
        v = strtod(buf, nullptr);
        return v;
    }

    JsonVal parse_value() {
        skip_ws();
        if (p >= end) return {};
        JsonVal v;
        if (*p == '"') {
            v.type = JsonVal::String;
            v.str  = parse_string();
        } else if (*p == '{') {
            v.type = JsonVal::Object;
            ++p;
            skip_ws();
            while (p < end && *p != '}') {
                skip_ws();
                if (*p != '"') { ++p; continue; }
                std::string key = parse_string();
                skip_ws();
                if (p < end && *p == ':') ++p;
                v.obj[key] = parse_value();
                skip_ws();
                if (p < end && *p == ',') ++p;
                skip_ws();
            }
            if (p < end) ++p;
        } else if (*p == '[') {
            v.type = JsonVal::Array;
            ++p;
            skip_ws();
            while (p < end && *p != ']') {
                v.arr.push_back(parse_value());
                skip_ws();
                if (p < end && *p == ',') ++p;
                skip_ws();
            }
            if (p < end) ++p;
        } else if (strncmp(p,"null",4)==0)  { v.type=JsonVal::Null;   p+=4; }
        else if (strncmp(p,"true",4)==0)    { v.type=JsonVal::Bool; v.b=true; p+=4; }
        else if (strncmp(p,"false",5)==0)   { v.type=JsonVal::Bool; v.b=false; p+=5; }
        else {
            v.type = JsonVal::Number;
            v.num  = parse_number();
        }
        return v;
    }
public:
    static JsonVal parse(const std::string& s) {
        JsonParser jp;
        jp.p   = s.data();
        jp.end = s.data() + s.size();
        return jp.parse_value();
    }
};
