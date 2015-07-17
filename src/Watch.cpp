// Copyright (C) 2014 - 2015 mru@sisyphus.teil.cc
//
// Watch interface: read data from device, parse it.
//
// Provides all parsed data via the "Callback" interface.
//


#include <ostream>
#include <iostream>
#include <algorithm>
#include <vector>
#include <list>
#include <iomanip>
#include <stdexcept>
#include <fstream>

#include <cassert>
#include <ctime>

#include "DataTypes.hpp"
#include "MemoryBlock.hpp"
#include "Callback.hpp"

#include "Watch.hpp"


static int16_t parse_int16(WatchMemoryBlock::mem_it_t it) {
    return *it + ( *(it+1) << 8 );
}

static int16_t parse_int16_advance(WatchMemoryBlock::mem_it_t& it) {
    auto val = parse_int16(it);
    it += 2;
    return val;
}

static int32_t parse_int32(WatchMemoryBlock::mem_it_t it) {
    return *it + ( *(it+1) << 8 ) + ( *(it+2) << 16 ) + ( *(it+3) << 24 );
}

static int32_t parse_int32_advance(WatchMemoryBlock::mem_it_t& it) {
    auto val = parse_int32(it);
    it += 4;
    return val;
}

Watch::Watch(std::shared_ptr<DeviceInterface> device) : br(), device(device) {
}

void Watch::addRecipient(std::shared_ptr<Callback> c) {
    br.addRecipient(c);
}

void Watch::parse() {
    parseBlock0();
}

void Watch::clearWorkouts() {
    // ensure we are connected
    std::string version = device->readVersion();

    // XXX don't read the block0 twice if read already.
    // as an future improvement.

    // read block 0, we need to write it back later
    WatchMemoryBlock cb(0, 1);
    readBlock(cb);

    std::fill(cb.memory.begin()+0xe0, cb.memory.begin()+0xe0+0x10, 0xff);
    *(cb.memory.begin()+0xe0) = 0xfe;

    // clear toc
    std::fill(cb.memory.begin()+0x100, cb.memory.begin()+0x100+0x100, 0xff);

    // download settings again
    writeBlock(cb);
}

void Watch::downloadEPO(const std::string& epo_fn) {
    //
    // ftp://ftp.leadtek.com/gps/9023/G-monitor%203.26/User%20Manual/EPO%20Format%20and%20Protocol/LEADTEK%20EPO%20Format%20and%20Protocol%20Reference%20Manual%20V1.0.pdf
    //
    // XXX not very trustworthy!
    // there are probably bugs in here.
    std::ifstream epo;
    epo.open(epo_fn, std::ios_base::in | std::ios_base::binary);
    if (! epo.good()) {
        throw std::runtime_error("Failed to open EPO file");
    }

    struct tm epo_eol;
    std::list<std::vector<char> > epo_entries;
    const size_t epo_entry_size = 0x3c;
    std::vector<char> epo_entry(epo_entry_size, 0x00);

    // parse epo file
    while ( epo.read(&epo_entry[0], epo_entry_size)) {
        epo_entries.push_back(epo_entry);
    }

    // only support MTK7d.EPO .. .7 days a 4 records a 32 entries
    if (epo_entries.size() != 896) {
        throw std::runtime_error("Failed to parse EPO file, wrong number of entries.");
    }


    // just a guess: the first three bytes of the entry 
    // contain the number of hours since the first GPS epoch, 
    // which started 06.jan.1980.
    int hours = (unsigned char)epo_entry[0] + ( ((unsigned char)epo_entry[1]) << 8) + ( ((unsigned char)epo_entry[2]) << 16);

    epo_eol.tm_sec =  epo_eol.tm_min = 0;
    epo_eol.tm_hour = hours;
    epo_eol.tm_mday = 6;
    epo_eol.tm_mon = 0;
    epo_eol.tm_year = 80;
    epo_eol.tm_wday = epo_eol.tm_yday = epo_eol.tm_isdst = 0;
    ::mktime(&epo_eol);

    std::cout << "EPO data valid until " << asctime(&epo_eol) << std::endl;


    std::fill(epo_entry.begin(), epo_entry.end(), 0);
    epo_entries.push_back(epo_entry);
    epo_entries.push_back(epo_entry);
    epo_entries.push_back(epo_entry);
    epo_entries.push_back(epo_entry);

    WatchMemoryBlock mb(0xe6, 1 + 0xf3 - 0xe6);

    WatchMemoryBlock::mem_it_t it = mb.memory.begin();

    int idx = 0;
    while (epo_entries.size()) {

        WatchMemoryBlock::mem_it_t cs_begin;

        // leader:
        *it++ = 0x04; *it++ = 0x24; *it++ = 0xbf; *it++ = 0x00;

        cs_begin = it;

        // data begin:
        *it++ = 0xd2; *it++ = 0x02;

        // index: 0xffff if last entry, else index
        if (epo_entries.size() == 3) {
            *it++ = 0xff; *it++ = 0xff;
        }
        else {
            *it++ = idx; *it++ = idx >> 8;
        }

        // copy 3 epo entries to the output
        for (int i = 0; i < 3; i++) {
            for (auto v: epo_entries.front()) {
                *it++ = v;
            }
            epo_entries.pop_front();
        }

        char cs = 0;
        while (cs_begin < it) {
            cs ^= *cs_begin++;
        }

        // trailer: checksum + \r\n
        *it++ = cs; *it++ = 0x0d; *it++ = 0x0a;
    }

    // some unknown data...
    *it++ = 0x2C; *it++ = 0x01; *it++ = 0x00; *it++ = 0x00;
    *it++ = 0x00; *it++ = 0x00; *it++ = 0x00; *it++ = 0x00;
    *it++ = 0x0D; *it++ = 0x00; *it++ = 0x00; *it++ = 0x00;
    *it++ = 0xF3; *it++ = 0x00; *it++ = 0x00; *it++ = 0x00;
    *it++ = 0x00; *it++ = 0x00; *it++ = 0x00; *it++ = 0x00;
    *it++ = 0xB0; *it++ = 0xEE; *it++ = 0x2E; *it++ = 0x02;
    *it++ = 0xDC; *it++ = 0xAE; *it++ = 0x57; *it++ = 0x00;
    *it++ = 0x00; *it++ = 0x00; *it++ = 0x00; *it++ = 0x00;
    *it++ = 0x01; *it++ = 0x00; *it++ = 0x00; *it++ = 0x00;
    *it++ = 0x00; *it++ = 0x00; *it++ = 0x00; *it++ = 0x00;
    *it++ = 0x00; *it++ = 0x00; *it++ = 0x00; *it++ = 0x00;

    // download data
    writeBlock(mb);

    // update "valid through" of agps data.
    // i think this value is only for display purposes (menu..gps..agps..expiry).
    device->setEpoEol(epo_eol.tm_year - 100, epo_eol.tm_mon + 1, epo_eol.tm_mday);
}


void Watch::parseGpsEle(GpsEle& l, WatchMemoryBlock::mem_it_t it) {
    l.ele = parse_int16(it);
}
void Watch::parseGpsLocation(GpsLocation& l, WatchMemoryBlock::mem_it_t it) {
    l.loc = parse_int32(it);
}
void Watch::parseGpsTimeUpd(GpsTimeUpd& t, WatchMemoryBlock::mem_it_t it) {
    t.mm = *it++;
    t.ss = *it++;
}
void Watch::parseGpsTime(GpsTime& t, WatchMemoryBlock::mem_it_t it, unsigned timezone) {
    // parses 6 bytes.
    t.time.tm_year = 100 + *it++;
    t.time.tm_mon = *it++ - 1;
    t.time.tm_mday = *it++;
    t.time.tm_hour = *it++;
    t.time.tm_min = *it++;
    t.time.tm_sec = *it++;
    t.time.tm_gmtoff = ((timezone - 24)/2)*3600 + ((timezone - 24)%2)*1800;
    t.mktime();
}
void Watch::parseSample(WatchInfo& wi, SampleInfo& si, WatchMemoryBlock::mem_it_t& it) {

    auto type = *it;
    si.fb = type;
    si.sb = *(it+1);
    switch (type) {
        case 0x00: 
            {
                si.type = SampleInfo::Full;
                si.hr = *(it+24);
                si.fix = si.sb;
                parseGpsTime(si.time, it+2, wi.timezone);
                parseGpsLocation(si.lon, it+8);
                parseGpsLocation(si.lat, it+12);
                parseGpsEle(si.ele, it+16);

                si.orientation = parse_int16(it+18);
                si.speed = parse_int16(it+20);
                si.distance = parse_int16(it+22);

                it += 25;
                break;
            }
        case 0x01: 
            {
                si.type = SampleInfo::Diff;
                si.hr = *(it+20);
                si.fix = si.sb;
                parseGpsTimeUpd(si.time_upd, it+2);
                parseGpsLocation(si.lon, it+4);
                parseGpsLocation(si.lat, it+8);
                parseGpsEle(si.ele, it+12);

                si.orientation = parse_int16(it+14);
                si.speed = parse_int16(it+16);
                si.distance = parse_int16(it+18);

                it += 21;
                break;
            }
        case 0x02: 
            {
                si.type = SampleInfo::TimeOnly;
                si.hr = 0;
                si.fix = 0;
                parseGpsTimeUpd(si.time_upd, it+1);
                it += 3;
                break;
            }
        case 0x03: 
            {
                si.type = SampleInfo::HrOnly;
                si.hr = *(it+7);
                si.fix = 0;
                parseGpsTime(si.time, it+1, wi.timezone);
                it += 8;
                break;
            }
        case 0x80: 
            {
                si.type = SampleInfo::None;
                si.hr = *(it+24);
                si.fix = si.sb;
                parseGpsTime(si.time, it+2, wi.timezone);
                parseGpsLocation(si.lon, it+8);
                parseGpsLocation(si.lat, it+12);
                parseGpsEle(si.ele, it+16);

                si.orientation = parse_int16(it+18);
                si.speed = parse_int16(it+20);
                si.distance = parse_int16(it+22);

                it += 25;
                break;
            }
        case 0xff: 
            {
                si.type = SampleInfo::End;
                it += 0;
                break;
            }
        default: 
            {
                assert(0 && type);
            }
    }
}

void Watch::parseWO(WatchInfo& wi, int first, int count) {

    WorkoutInfo wo;
    WatchMemoryBlock cb(first, count);
    readBlock(cb);

    WatchMemoryBlock::mem_it_t it = cb.memory.begin();

    // 2 [0..1]
    wo.nsamples = parse_int16_advance(it);  

    // 1 [2]
    wo.lapcount = *it++;  

    // 6 [3..8]
    std::vector<unsigned char> reverse_time(6);
    std::reverse_copy(it, it +6, reverse_time.begin());
    parseGpsTime(wo.start_time, reverse_time.begin(), wi.timezone);
    it += 6;

    // 3 [9..11] workout time
    std::reverse_copy(it, it +3, reverse_time.begin()+3);
    parseGpsTime(wo.workout_time, reverse_time.begin(), wi.timezone);
    it += 3;

    // 3 [12..14] ?
    it += 3;

    // 1 [15]
    wo.profile = *it++;

    // 4 [16..19]
    wo.total_km = parse_int32_advance(it);

    // 2 [20..21]
    wo.speed_avg = parse_int16_advance(it);

    // 2 [22..23]
    wo.speed_max = parse_int16_advance(it);

    // 4 [24..27] ???
    it += 4;

    // 28, 29, 30
    wo.hr_avg = *it++;
    wo.hr_max = *it++;
    wo.hr_min = *it++;

    // 1 [31] ???
    it++;

    // 4 [32..35]
    wo.calories = parse_int32_advance(it); 

    // 3 [36..38] time below training zone
    std::reverse_copy(it, it +3, reverse_time.begin()+3);
    parseGpsTime(wo.below_zone_time, reverse_time.begin(), wi.timezone);
    it += 3;

    // 1 [39] ?
    it++;

    // 3 [40..42] time in training zone
    std::reverse_copy(it, it +3, reverse_time.begin()+3);
    parseGpsTime(wo.in_zone_time, reverse_time.begin(), wi.timezone);
    it += 3;

    // 1 [43] ?
    it++;

    // 3 [44..46] time above training zone
    std::reverse_copy(it, it +3, reverse_time.begin()+3);
    parseGpsTime(wo.above_zone_time, reverse_time.begin(), wi.timezone);

    it = cb.memory.begin() + 64;

    //now every 16bytes is a laptime 1=hour 2=minutes 3=seconds 4+5=microseconds 10+9 lap distance 13=lapspeed in km/h

    br.onWorkout(wo);

    it = cb.memory.begin() + 0x1000;
    bool track_active = false;
    GpsTime t;
    GpsLocation lon;
    GpsLocation lat;
    GpsEle ele;
    unsigned idx_wo = 0;
    unsigned idx_track = 0;
    bool has_full_fix = false;


    // something is wrong with nsamples - trusting "0xff" works better
    for (unsigned i =0; /*i< wo.nsamples*/;i++) {

        assert (it < cb.memory.end());
        SampleInfo si;
        parseSample(wi, si, it);
        idx_wo ++;
        idx_track++;


        if (si.type == SampleInfo::None || si.type == SampleInfo::Full || si.type == SampleInfo::HrOnly) {
            t = si.time;
        }
        else if (si.type == SampleInfo::Diff || si.type == SampleInfo::TimeOnly) {
            // special case: minute overrun. 
            // this condition is probably incorrect.
            // XXX find a proper way to fix this
            // it helps in a lot of cases
            // DST is not covered by this, is DST really a problem: to be confirmed
            if (si.time_upd.mm < t.time.tm_min) {
                t.time.tm_hour ++;
                // mktime fixes clock info, 
                // 23:50 + 1h = 24:50; mktime increments day automatically.
                mktime(&t.time);
            }
            si.time = (t = si.time_upd);
        }
        else if (si.type == SampleInfo::End) {
            break;
        }


        bool has_position = false;
        if (si.type == SampleInfo::Full) {
            lon = si.lon;
            lat = si.lat;
            ele = si.ele;
            has_full_fix = true;
            has_position = true;
        }
        else if (si.type == SampleInfo::Diff) {
            si.lon = (lon += si.lon);
            si.lat = (lat += si.lat);
            si.ele = (ele += si.ele);
            has_position = has_full_fix;
        }
        else if (si.type == SampleInfo::None) {
            if (si.fix) {
                // not sure about this
                lon = si.lon;
                lat = si.lat;
                ele = si.ele;
            }
            has_position = has_full_fix = si.fix != 0;
        }
        else {
            // certainly not:
            // there can be a HrOnly/0x03, followed only by a Diff/0x01
            //has_position = has_full_fix = false;
        }


        if (!has_position) {
            si.fix = 0;
        }

        if (track_active && si.type == SampleInfo::None) {
            track_active = false;
            TrackInfo i;
            br.onTrackEnd(i);
        }


        if (!track_active) {
            TrackInfo t;
            br.onTrack(t);
            track_active = true;
            idx_track = 1;
        }

        si.idx_track = idx_track;
        si.idx_wo = idx_wo;
        br.onSample(si);

    }

    if (track_active) {
        TrackInfo i;
        br.onTrackEnd(i);
    }


    br.onWorkoutEnd(wo);
}

void Watch::parseToc(Toc& toc, WatchMemoryBlock::mem_it_t it) {
    toc.clear();

    for (;;) {
        std::ostringstream ss;

        unsigned char cur;
        unsigned char first;
        cur = first = *it++;
        if (first == 0xff) {
            break;
        }

        ss << (int)cur;
        while (*it != 0xff) {
            cur = *it++;
            ss << "," << (int)cur;
        }

        toc.push_back(1+cur-first);
        ++it;
    }
}

void Watch::parseBlock0() {

    WatchInfo wi;

    wi.version = device->readVersion();

    // currently disabled, https://github.com/mru00/crane_gps_watch/issues/16
    //wi.version2 = device->readVersion2();

    WatchMemoryBlock mb(0, 1);
    readBlock(mb);

    WatchMemoryBlock::mem_it_t mem_it = mb.memory.begin();

    unsigned char cs = *mem_it;
    unsigned char cs_v = *(mem_it+1);
    if ( cs != (unsigned char)(~cs_v) ) {
        throw std::runtime_error ("checksum mismatch");
    }

    // timezones:
    // - 1:00 = 0x16
    //   0:00 = 0x18
    // + 1:00 = 0x1a
    // + 2:00 = 0x1c
    // + 2:30 = 0x1d
    wi.timezone = *(mem_it + 3);
    wi.sample_interval = *(mem_it + 14);
    wi.selected_profile = *(mem_it + 0x10+10);

    wi.language = *(mem_it + 0x50 + 13);

    mem_it += 0x60;
    wi.firmware.resize(16, '\0');
    std::copy(mem_it, mem_it+16, wi.firmware.begin());



    br.onWatch(wi);

    mem_it = mb.memory.begin() + 0x100;
    parseToc(wi.toc, mem_it);

    int f = 1;
    for (auto t : wi.toc) {
        parseWO(wi, f, t);
        f+=t;
    }

    br.onWatchEnd(wi);
}

void Watch::readBlock(WatchMemoryBlock& b) {
    // read b.count bytes from block# b.id
    const unsigned readSize = 0x80;
    const unsigned rcount = b.blockSize / readSize;

    br.onReadBlocks(b.id, b.count);

    WatchMemoryBlock::mem_it_t mem_it = b.memory.begin();
    for (unsigned block = 0; block < b.count; block++) {
        unsigned block_start = (b.id+block)*b.blockSize;

        unsigned char* block_begin = &* mem_it;
        for (unsigned nbyte = 0; nbyte < rcount; nbyte++) {
            device->readMemory(block_start + nbyte*readSize, readSize, &*mem_it);
            mem_it += readSize;
        }
        br.onReadBlock(b.id+block, block_start, block_begin, b.blockSize);
    }

    if (0) {
        b.dump();
    }
}

void Watch::writeBlock(WatchMemoryBlock& b) {
    // read b.count bytes from block# b.id
    const unsigned readSize = 0x80;
    const unsigned rcount = b.blockSize / readSize;

    WatchMemoryBlock::mem_it_t mem_it = b.memory.begin();
    for (unsigned block = 0; block < b.count; block++) {


        unsigned block_start = (b.id+block)*b.blockSize;

        // enables clear flash, clearFlash2 fails without clearFlash1
        device->clearFlash1();

        // really clears flash - everything, also settings
        device->clearFlash2(block_start);


        for (unsigned nbyte = 0; nbyte < rcount; nbyte++) {

            // special case for write settings after flash clear:
            // only download the first 0x100 bytes, not the full segment

            if (mem_it >= b.memory.end()) return;

            device->writeMemory(block_start + nbyte*readSize, readSize, &*mem_it);
            mem_it += readSize;
        }
    }
}

