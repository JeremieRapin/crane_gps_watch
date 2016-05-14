// Copyright (C) 2014 - 2015 mru@sisyphus.teil.cc


#include <stack>
#include <stdexcept>
#include <cassert>
#include <iostream>
#include <sstream>

#include <cstring>


#include "XmlFileWriter.hpp"


unsigned char* get_str(const std::string& str) {
    return (unsigned char*) str.c_str();
}

XmlFileWriter::XmlFileWriter() : wf(nullptr), w(nullptr) {
}

XmlFileWriter::~XmlFileWriter() {
    // XXX shouldn't happen
    if (isOpen()) {
        close();
    }
}

void XmlFileWriter::open(const std::string& filename) {
    std::cerr << "XmlFileWriter: writing to " << filename << std::endl;
    wf = fopen(filename.c_str(), "w");
    if (wf == nullptr) {
        throw std::runtime_error(strerror(errno));
    }
    fprintf(wf, "<?xml version=\"1.0\"?>\n");
    w = genxNew(NULL, NULL, NULL);
}

bool XmlFileWriter::isOpen() const {
    return w != nullptr;
}

void XmlFileWriter::close() {
    if (!isOpen()) {
        throw std::runtime_error("Protocol mismatch: closing already closed XML document");
    }
    genxDispose(w);
    fclose(wf);
    wf = nullptr;
    w = nullptr;
}

void XmlFileWriter::startDocument(const std::string&) {
    genxStartDocFile(w, wf);
}
void XmlFileWriter::endDocument() {
    genxEndDocument(w);
}

void XmlFileWriter::startElement(const std::string& name) {
    genxAddCharacter(w, '\n');
    for(unsigned int i = 0; i < stack.size(); i++) {
        genxAddCharacter(w, ' ');
    }
    genxStartElementLiteral(w, NULL, get_str(name));
    stack.push(name);
}

void XmlFileWriter::endElement(const std::string& name) {
    genxAddCharacter(w, '\n');
    for(unsigned int i = 1; i < stack.size(); i++) {
        genxAddCharacter(w, ' ');
    }
    genxEndElement(w);
    if (name.size()) {
        if (name != stack.top()) throw std::runtime_error("XML Tag mismatch (expected a <" + stack.top() + ">, got a <" + name +">)");
    }
    stack.pop();
}

void XmlFileWriter::writeAttribute(const std::string& name, const std::string& value) {
    genxAddAttributeLiteral(w, NULL, get_str(name), get_str(value));
}

void XmlFileWriter::writeElement(const std::string& name, const std::string& value) {
    genxAddCharacter(w, '\n');
    for(unsigned int i = 0; i < stack.size(); i++) {
        genxAddCharacter(w, ' ');
    }
    genxStartElementLiteral(w, NULL, get_str(name));
    genxAddText(w, get_str(value));
    genxEndElement(w);
}
