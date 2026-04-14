/*
////////////////////////////////////////////////////////////////////////////
////////////  Choose disk image for given slot number? ////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
//// Adapted for linapple - apple][ emulator for Linux by beom beotiger Nov 2007   /////
///////////////////////////////////////////////////////////////////////////////////////
// Original source from one of Brain Games (http://www.braingames.getput.com)
//  game Super Transball 2.(http://www.braingames.getput.com/stransball2/default.asp)
//
// Brain Games crew creates brilliant retro-remakes! Please visit their site to find out more.
//
*/

/* March 2012 AD by Krez, Beom Beotiger */

#include "core/Common.h"
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <sys/stat.h>
#include <ctime>
#include <vector>
#include <cstdio>
#include <string>
#include <algorithm>
#include <array>
#include <memory>

#include "core/file_entry.h"
#include "apple2/DiskFTP.h"
#include "apple2/ftpparse.h"
#include "core/Common_Globals.h"
#include "core/Registry.h"
#include "frontends/sdl3/DiskChoose.h"

// how many file names we are able to see at once!
static constexpr int FILES_IN_SCREEN = 21;
// delay after key pressed (in milliseconds??)
static constexpr int KEY_DELAY = 25;
// define time when cache ftp dir.listing must be refreshed
static constexpr int RENEW_TIME = 24 * 3600;

auto md5str(const char *input) -> char *; // forward declaration of md5str func

std::array<char, 512> g_sFTPDirListing = {{"cache/ftp."}}; // name for FTP-directory listing
auto getstatFTP(struct ftpparse *fp, uintmax_t *size) -> int
{
  // gets file status and returns: 0 - special or error, 1 - file is a directory, 2 - file is a normal file
  // In: fp - ftpparse struct ftom ftpparse.h
  if (!fp->namelen) {
    return 0;
  }
  if (fp->flagtrycwd == 1) {
    return 1;  // can CWD, it is dir then
  }

  if (fp->flagtryretr == 1) { // we're able to RETR, it's a file then?!
    if (size != nullptr) {
      *size = fp->size / 1024;
    }
    return 2;
  }
  return 0;
}


struct FTP_file_list_generator_t : public file_list_generator_t {
  FTP_file_list_generator_t(const std::string& dir) :
    directory(dir)
  {}

  auto generate_file_list() -> const std::vector<file_entry_t> override;

  auto get_starting_message() -> const std::string override {
    return "Connecting to FTP server... Please wait.";
  }

  auto get_failure_message() -> const std::string override {
    return failure_message;
  }

private:
  std::string directory;
  std::string failure_message = "(no info)";
};


auto FTP_file_list_generator_t::generate_file_list() -> const std::vector<file_entry_t> {
  std::array<char, 1024> ftpdirpath;
  int l = snprintf(ftpdirpath.data(), ftpdirpath.size(), "%s/%s%s", g_state.sFTPLocalDir, g_sFTPDirListing.data(), md5str(directory.c_str())); // get path for FTP dir listing

  if (l<0 || static_cast<size_t>(l)>=ftpdirpath.size()) {      // check returned value
    failure_message = "Failed get path for FTP dir listing";
    return {};
  }


  bool OKI = false;
  struct stat info{};
  if (stat(ftpdirpath.data(), &info) == 0 && info.st_mtime > time(nullptr) - RENEW_TIME) {
    OKI = false; // use this file
  } else {
    OKI = ftp_get(directory.c_str(), ftpdirpath.data()); // get ftp dir listing
  }

  if (OKI) {  // error
    failure_message =
      "Failed getting FTP directory " + directory + " to " + std::string(ftpdirpath.data());
    return {};
  }

  std::vector<file_entry_t> file_list;

  // build prev dir
  if (directory != "ftp://") {
    file_list.emplace_back( "..", file_entry_t::UP, 0 );
  }

  FilePtr fdir(fopen(ftpdirpath.data(), "r"), fclose);
  if (!fdir) {
    failure_message = "Failed to open FTP directory listing file: " + std::string(ftpdirpath.data());
    return {};
  }
  char *tmp = nullptr;
  std::array<char, 512> tmpstr;
  while ((tmp = fgets(tmpstr.data(), static_cast<int>(tmpstr.size()), fdir.get()))) // first looking for directories
  {
    // clear and then try to fill in FTP_PARSE struct
    struct ftpparse FTP_PARSE{}; // for parsing ftp directories

    memset(&FTP_PARSE, 0, sizeof(FTP_PARSE));
    ftpparse(&FTP_PARSE, tmp, strlen(tmp));

    uintmax_t fsize = 0;
    const int what = getstatFTP(&FTP_PARSE, &fsize);
    if (strlen(FTP_PARSE.name) < 1) {
      continue;
    }

    std::unique_ptr<char, void(*)(void*)> trimmed_name(php_trim(FTP_PARSE.name, strlen(FTP_PARSE.name)), free);

    switch (what) {
    case 1: // is directory!
      file_list.emplace_back( trimmed_name.get(), file_entry_t::DIR, 0 );
      break;

    case 2: // is normal file!
      file_list.emplace_back( trimmed_name.get(), file_entry_t::FILE, fsize*1024 );
      break;

    default: // others: simply ignore
      ;
    }
  }

  std::sort(file_list.begin(), file_list.end());

  return file_list;
}


auto ChooseAnImageFTP(int sx, int sy, const std::string& ftp_dir, int slot,
                      std::string& filename, bool& isdir, size_t& index_file) -> bool
{
  /*  Parameters:
   sx, sy - window size,
   ftp_dir - what FTP directory to use,
   slot - in what slot should an image go (common: #6 for 5.25' 140Kb floppy disks, and #7 for hard-disks).
     slot #5 - for 800Kb floppy disks, but we do not use them in Apple][?
    (They are as a rule with .2mg extension)
   index_file  - from which file we should start cursor (should be static and 0 when changing dir)

   Out:  filename  - chosen file name (or dir name)
    isdir    - if chosen name is a directory
  */

  FTP_file_list_generator_t file_list_generator(ftp_dir);
  return ChooseImageDialog(sx, sy, ftp_dir, slot, &file_list_generator,
                           filename, isdir, index_file);
}

/* md5.c - an implementation of the MD5 algorithm and MD5 crypt */
/* See RFC 1321 for a description of the MD5 algorithm.
 */
#define cpu_to_le32(x) (x)
#define le32_to_cpu(x) cpu_to_le32(x)
using UINT4 = uint32_t;

/* F, G, H and I are basic MD5 functions.
 */
#define F(x, y, z) (((x) & (y)) | ((~x) & (z)))
#define G(x, y, z) (((x) & (z)) | ((y) & (~z)))
#define H(x, y, z) ((x) ^ (y) ^ (z))
#define I(x, y, z) ((y) ^ ((x) | (~z)))

/* ROTATE_LEFT rotates x left n bits.
 */
#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x >> (32 - (n)))))

static const std::array<UINT4, 4> md5_initstate = {{0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476}};

static const std::array<char, 4> s1 = {{7, 12, 17, 22}};
static const std::array<char, 4> s2 = {{5, 9, 14, 20}};
static const std::array<char, 4> s3 = {{4, 11, 16, 23}};
static const std::array<char, 4> s4 = {{6, 10, 15, 21}};

static const std::array<UINT4, 64> T = {{0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
                      0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
                      0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
                      0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
                      0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
                      0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
                      0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
                      0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391}};

static std::array<UINT4, 4> state;
static uint32_t length;
static std::array<uint8_t, 64> buffer;

static void md5_transform(const uint8_t block[64])
{
  int i = 0, j = 0;
  UINT4 a = 0, b = 0, c = 0, d = 0, tmp = 0;
  const auto *x = reinterpret_cast<const UINT4 *>(block);

  a = state[0];
  b = state[1];
  c = state[2];
  d = state[3];

  /* Round 1 */
  for (i = 0; i < 16; i++) {
    tmp = a + F (b, c, d) + le32_to_cpu (x[static_cast<size_t>(i)]) + T[static_cast<size_t>(i)];
    tmp = ROTATE_LEFT (tmp, s1[static_cast<size_t>(i & 3)]);
    tmp += b;
    a = d;
    d = c;
    c = b;
    b = tmp;
  }
  /* Round 2 */
  for (i = 0, j = 1; i < 16; i++, j += 5) {
    tmp = a + G (b, c, d) + le32_to_cpu (x[static_cast<size_t>(j & 15)]) + T[static_cast<size_t>(i + 16)];
    tmp = ROTATE_LEFT (tmp, s2[static_cast<size_t>(i & 3)]);
    tmp += b;
    a = d;
    d = c;
    c = b;
    b = tmp;
  }
  /* Round 3 */
  for (i = 0, j = 5; i < 16; i++, j += 3) {
    tmp = a + H (b, c, d) + le32_to_cpu (x[static_cast<size_t>(j & 15)]) + T[static_cast<size_t>(i + 32)];
    tmp = ROTATE_LEFT (tmp, s3[static_cast<size_t>(i & 3)]);
    tmp += b;
    a = d;
    d = c;
    c = b;
    b = tmp;
  }
  /* Round 4 */
  for (i = 0, j = 0; i < 16; i++, j += 7) {
    tmp = a + I (b, c, d) + le32_to_cpu (x[static_cast<size_t>(j & 15)]) + T[static_cast<size_t>(i + 48)];
    tmp = ROTATE_LEFT (tmp, s4[static_cast<size_t>(i & 3)]);
    tmp += b;
    a = d;
    d = c;
    c = b;
    b = tmp;
  }

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
}

static void md5_init() {
  memcpy(state.data(), md5_initstate.data(), sizeof(md5_initstate));
  length = 0;
}

static void md5_update(const char *input, int inputlen) {
  int buflen = static_cast<int>(length & 63);
  length += static_cast<uint32_t>(inputlen);
  if (buflen + inputlen < 64) {
    memcpy(buffer.data() + buflen, input, static_cast<size_t>(inputlen));
    return;
  }

  memcpy(buffer.data() + buflen, input, static_cast<size_t>(64 - buflen));
  md5_transform(buffer.data());
  input += 64 - buflen;
  inputlen -= 64 - buflen;
  while (inputlen >= 64) {
    md5_transform(reinterpret_cast<const uint8_t *>(input));
    input += 64;
    inputlen -= 64;
  }
  memcpy(buffer.data(), input, static_cast<size_t>(inputlen));
}

static auto md5_final() -> uint8_t *
{
  int buflen = static_cast<int>(length & 63);

  buffer[static_cast<size_t>(buflen++)] = 0x80;
  memset(buffer.data() + buflen, 0, static_cast<size_t>(64 - buflen));
  if (buflen > 56) {
    md5_transform(buffer.data());
    memset(buffer.data(), 0, 64);
    buflen = 0;
  }

  *reinterpret_cast<UINT4 *>(buffer.data() + 56) = cpu_to_le32 (8 * length);
  *reinterpret_cast<UINT4 *>(buffer.data() + 60) = 0;
  md5_transform(buffer.data());

  for (size_t i = 0; i < 4; i++) {
    state[i] = cpu_to_le32 (state[i]);
  }
  return reinterpret_cast<uint8_t *>(state.data());
}

static auto md5(const char *input) -> char * {
  md5_init();
  md5_update(input, static_cast<int>(strlen(input)));
  return reinterpret_cast<char *>(md5_final());
}

// GPH Warning: Not re-entrant!
auto md5str(const char *input) -> char * {
  static std::array<char, 16 * 3 + 1> result;
  auto *digest = reinterpret_cast<uint8_t *>(md5(input));

  for (size_t i = 0; i < 16; i++) {
    sprintf(result.data() + static_cast<ptrdiff_t>(2 * i), "%02X", digest[i]);
  }
  return result.data();
}
