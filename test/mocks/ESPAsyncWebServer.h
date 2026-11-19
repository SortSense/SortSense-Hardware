#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string>

class AsyncWebServerResponse {
public:
  virtual ~AsyncWebServerResponse() {}
  virtual void addHeader(const std::string &name, const std::string &value) {
    (void)name;
    (void)value;
  }
};

class AsyncAbstractResponse : public AsyncWebServerResponse {
protected:
  int _code;
  const char *_contentType;
  size_t _contentLength;

public:
  AsyncAbstractResponse(void * /*arg*/ = nullptr)
      : _code(200), _contentType(""), _contentLength(0) {}
  virtual ~AsyncAbstractResponse() override {}

  virtual size_t _fillBuffer(uint8_t *data, size_t len) = 0;
  virtual bool _sourceValid() const { return true; }

  int getCode() const { return _code; }
  const char *getContentType() const { return _contentType; }
  size_t getContentLength() const { return _contentLength; }
};
