#include "csvc17.h"

class csv_parser_t {
private:
  void reset() {
    if (m_csv.__internal) {
      csv_close(&m_csv);
      m_csv = {};
    }
    m_csv = csv_open(m_conf);
  }
public:
  ~csv_parser_t() { csv_close(&m_csv); }

  csv_parser_t(csv_parser_t&) = delete;
  csv_parser_t(csv_parser_t&&) = delete;
  void operator=(csv_parser_t&) = delete;
  void operator=(csv_parser_t&&) = delete;

  bool ok() const { return m_csv.ok; }
  const char* errmsg() const { return m_csv.errmsg; }

  // set parameters
  csv_parser_t& set_delim(char delim) {
    m_conf.delim = delim;
    return *this;
  }
  csv_parser_t& set_quote(char qte) {
    m_conf.qte = qte;
    return *this;
  }
  csv_parser_t& set_escape(char esc) {
    m_conf.esc = esc;
    return *this;
  }
  csv_parser_t& set_nullstr(std::string_view nullstr) {
    int sz = nullstr.size();
    if (sz >= sizeof(m_conf.nullstr) - 1) {
      sz = sizeof(m_conf.nullstr) - 1;
    }
    std::memcpy(m_conf.nullstr, nullstr.data(), sz);
    m_conf.nullstr[sz] = '\0';
    return *this;
  }
  csv_parser_t& set_initbufsz(int n) {
    m_conf.initbufsz = n;
    return *this;
  }
  csv_parser_t& set_maxbufsz(int n) {
    m_conf.maxbufsz = n;
    return *this;
  }

  // really parse the csv data
  bool parse_file(FILE* fp, csv_perrow_t* perrow) {
    reset();
    return 0 == csv_parse_file(&m_csv, fp, this, perrow);
  }
  bool parse_file(std::string_view path, csv_perrow_t* perrow) {
    reset();
    return 0 == csv_parse_file_ex(&m_csv, path, this, perrow);
  }
  bool parse(csv_feed_t* feed, csv_perrow_t* perrow) {
    reset();
    return 0 == csv_parse(&m_csv, this, feed, perrow);
  }
  
private:
  csv_t m_csv = {};
  csv_config_t m_conf = csv_default_config();
};

