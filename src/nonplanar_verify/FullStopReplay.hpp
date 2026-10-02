#pragma once
// Independent native-byte state machine. No writer/planner/GCodeProcessor API.
#include <array>
#include <chrono>
#include <cfenv>
#include <cmath>
#include <functional>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace nptop_verify {
enum class FullStopKind { XYZ, Pressure, Dwell };
struct FullStopMove {
    FullStopKind kind;
    std::array<double,3> start,end;
    double e,feed,acceleration,dwell_seconds;
};
struct FullStopReplayLimits {
    size_t max_bytes=32*1024*1024,max_events=200000;
    std::chrono::milliseconds timeout{1000};
    std::function<bool()> cancelled;
};
inline std::vector<FullStopMove> replay_full_stop(const std::string &bytes,std::array<double,3> initial,
    const FullStopReplayLimits &requested_limits={})
{
    const auto limits=requested_limits;const auto started=std::chrono::steady_clock::now();
    const auto check=[](bool ok) {if (!ok) throw std::invalid_argument("unsupported or malformed full-stop candidate");};
    check(limits.max_bytes && limits.max_bytes<=32*1024*1024 && limits.max_events && limits.max_events<=200000 &&
          limits.timeout.count()>0 && limits.timeout<=std::chrono::seconds(30) && bytes.size()<=limits.max_bytes && !bytes.empty() && bytes.back()=='\n');
    for(double value : initial) check(std::isfinite(value) && std::abs(value)<=10000);
    const auto stop=[&] {check(!limits.cancelled || !limits.cancelled());check(std::fegetround()==FE_TONEAREST);check(std::chrono::steady_clock::now()-started<limits.timeout);};
    const auto number=[&](const std::string &word) {
        check(!word.empty());size_t j=0;if(word[j]=='-' || word[j]=='+')++j;
        bool digit=false,dot=false;
        for(;j<word.size();++j) {
            if(word[j]=='.' && !dot)dot=true;
            else {check(word[j]>='0' && word[j]<='9');digit=true;}
        }
        check(digit);std::istringstream input(word);input.imbue(std::locale::classic());double value=0;
        check(bool(input>>value) && std::isfinite(value));return value;
    };
    std::istringstream input(bytes);input.imbue(std::locale::classic());std::string line;
    unsigned header=0;double acceleration=0,pressure_debt=0;bool pending=false;std::vector<FullStopMove> moves;
    while(std::getline(input,line)) {
        stop();check(!line.empty() && line.size()<=256);std::istringstream row(line);row.imbue(std::locale::classic());std::string command,word;
        check(bool(row>>command));
        if(header<3) {
            check(command==(header==0 ? "G90" : header==1 ? "M83" : "M400") && !(row>>word));++header;continue;
        }
        if(command=="M204") {
            check(header==3 && !pending && moves.empty());check(bool(row>>word) && word.size()>1 && word[0]=='S');
            acceleration=number(word.substr(1));check(acceleration>0 && acceleration<=1000000 && !(row>>word));++header;continue;
        }
        check(header==4);
        if(command=="M400") {check(pending && !(row>>word));pending=false;continue;}
        check(!pending && moves.size()<limits.max_events);
        if(command=="G4") {
            check(bool(row>>word) && word.size()>1 && word[0]=='P');const double milliseconds=number(word.substr(1));
            check(milliseconds>0 && milliseconds<=1000000 && !(row>>word));
            moves.push_back({FullStopKind::Dwell,initial,initial,0,0,acceleration,milliseconds/1000});pending=true;continue;
        }
        check(command=="G1");bool seen[5]={};auto end=initial;double e=0,feed=0;
        while(row>>word) {
            check(word.size()>1);const size_t axis=std::string("XYZEF").find(word[0]);check(axis!=std::string::npos && !seen[axis]);seen[axis]=true;
            const double value=number(word.substr(1));
            if(axis<3){check(std::abs(value)<=10000);end[axis]=value;}
            else if(axis==3){check(value!=0 && std::abs(value)<=10000);e=value;}
            else {check(value>0 && value<100000);feed=value;}
        }
        check(seen[4]);const bool xyz=seen[0] || seen[1] || seen[2];
        if(xyz) {
            check(seen[0] && seen[1] && seen[2] && end!=initial && e>=0 && (e==0 || pressure_debt==0));
        } else {
            check(seen[3]);
            if(e<0){check(pressure_debt==0);pressure_debt=-e;}
            else {check(pressure_debt==e);pressure_debt=0;}
        }
        moves.push_back({xyz ? FullStopKind::XYZ : FullStopKind::Pressure,initial,end,e,feed,acceleration,0});initial=end;pending=true;
    }
    stop();check(header==4 && !pending && !moves.empty());return moves;
}
}
