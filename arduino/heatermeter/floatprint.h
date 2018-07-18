// HeaterMeter Copyright 2018 Bryan Mayland <bmayland@capnbry.net>
// Simple class to print a float right-justified in a given space
#ifndef __FLOATPRINT_H__
#define __FLOATPRINT_H__

template <size_t bufsize> class FloatPrint : public Print
{
public:
  FloatPrint(void) {}
  using Print::write;
  virtual size_t write(uint8_t c) { if (pos < bufsize && pos < wid) buf[pos++] = c; return 1; }

  void print(Print &p, double n, uint8_t width, uint8_t prec)
  {
    pos = 0;
    wid = width;
    Print::print(n, prec); // will update pos and buf

    uint8_t r = width - pos;
    while (r--) p.write(' ');
    r = 0;
    while (r < pos) p.write(buf[r++]);
  }

  void println(Print &p, double n, uint8_t width, uint8_t prec)
  {
    print(p, n, width, prec);
    p.println();
  }

private:
  char buf[bufsize];
  uint8_t pos;
  uint8_t wid;
};

#endif