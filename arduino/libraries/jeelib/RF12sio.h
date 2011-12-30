// Streaming I/O layer on top of RF12 driver
// 2009-05-07 <jc@wippler.nl> http://opensource.org/licenses/mit-license.php

class RF12 {
    enum { F_EXT = -6, F_STR, F_8, F_4, F_2, F_1 };
    
    MilliTimer sendTimer, ackTimer;
    uint8_t txbase, txfill, txfields, txPending;
    uint8_t txbuf[138]; // one full packet plus room for constructing next one
    uint8_t rxpos, rxfield, ackHead, ackData;

    void initBuf(uint8_t base);
    void addToBuf(uint8_t type, const void *ptr, uint8_t len);
    
public:
    RF12 () : txPending (0), ackHead (0) { initBuf(0); }
    
    RF12& put(const void*, char);
    RF12& get(void*, char);
    uint8_t read() { char v; get(&v, F_1); return v; }
                                                
    RF12& operator<< (char v)                   { return put(&v, F_1); }
    RF12& operator<< (unsigned char v)          { return put(&v, F_1); }
    RF12& operator<< (int v)                    { return put(&v, F_2); }
    RF12& operator<< (unsigned v)               { return put(&v, F_2); }
    RF12& operator<< (long v)                   { return put(&v, F_4); }
    RF12& operator<< (unsigned long v)          { return put(&v, F_4); }
    RF12& operator<< (long long v)              { return put(&v, F_8); }
    RF12& operator<< (unsigned long long v)     { return put(&v, F_8); }
    RF12& operator<< (float v)                  { return put(&v, F_4); }
    RF12& operator<< (double v)                 { return put(&v, F_8); }
    RF12& operator<< (const char* v)            { return put(v, F_STR); }
    RF12& operator<< (const unsigned char* v)   { return put(v, F_STR); }
                          
    // max payload is one string arg of 63 chars: needs 64b, plus 8 for fields
    uint8_t ready() const { return txfill <= sizeof txbuf - 72; }
    
    void send(uint8_t v);

    RF12& operator>> (char& v)                  { return get(&v, F_1); }
    RF12& operator>> (unsigned char& v)         { return get(&v, F_1); }
    RF12& operator>> (int& v)                   { return get(&v, F_2); }
    RF12& operator>> (unsigned& v)              { return get(&v, F_2); }
    RF12& operator>> (long& v)                  { return get(&v, F_4); }
    RF12& operator>> (unsigned long& v)         { return get(&v, F_4); }
    RF12& operator>> (long long& v)             { return get(&v, F_8); }
    RF12& operator>> (unsigned long long& v)    { return get(&v, F_8); }
    RF12& operator>> (float& v)                 { return get(&v, F_4); }
    RF12& operator>> (double& v)                { return get(&v, F_8); }
    RF12& operator>> (char* v)                  { return get(v, F_STR); }
    RF12& operator>> (unsigned char* v)         { return get(v, F_STR); }
    
    uint8_t poll();
    uint8_t nextSize();
    
    void to(uint8_t node)                       {}
    uint8_t from()                              { return 0; }
};
