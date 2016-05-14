// Copyright (C) 2014 - 2015 mru@sisyphus.teil.cc
//


#pragma once

#include <stack>

#include "genx.h"


class XmlFileWriter {
    XmlFileWriter(const XmlFileWriter&) = delete;
    XmlFileWriter& operator=(const XmlFileWriter&) = delete;
  public:
    XmlFileWriter();
    ~XmlFileWriter();

    void open(const std::string& filename);
    bool isOpen() const;
    void close();
    void startDocument(const std::string& encoding = "UTF-8");
    void startElement(const std::string& name);
    void writeAttribute(const std::string& name, const std::string& value);
    void writeElement(const std::string& name, const std::string& value);
    void endElement(const std::string& name = "");
    void endDocument();

  private:
    FILE* wf;
    genxWriter w;
    std::stack<std::string> stack;
};
