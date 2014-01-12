#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>

#include <windows.h>

#include "curl/curl.h"
#include "dispatcher.h"

using std::cin;
using std::cout;
using std::endl;

struct UploadInfo
{
    std::vector<std::string> upload_text;
    int current_index;
};

std::string Base64Encode(const std::string& source)
{
    static const char* base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
      unsigned char char_array_3[3], char_array_4[4];
      int i = 0, j = 0;
  
      std::string result(2000, '\0');
      int len = source.length() + 1;
      char* chsrc = new char[len];
      memcpy(chsrc, &source[0], len - 1);
      chsrc[len - 1] = '\0';
      char* chdes = &result[0];
      while(len--)
      {
            char_array_3[i++] = *(chsrc++);
            if(3 == i)
            {
                  char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
                  char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
                  char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
                  char_array_4[3] = char_array_3[2] & 0x3f;
                  for(i = 0; i < 4; i++)
                        *(chdes+i) = base64_chars[char_array_4[i]];
     
                  i = 0;
                 chdes += 4;
            }
      }
      if(i)
      {
             for(j = i; j < 3; j++)
             char_array_3[j] = '\0';
   
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;
   
            for(j = 0; j < (i+1); j++)
                  *(chdes++) = base64_chars[char_array_4[j]];
    
            while((3 > i++))
                  *(chdes++) = '=';
      }
  
      *chdes = '\0';
      return result.substr(0, strlen(&result[0]));
}

size_t WriteCallback(const void* buf, size_t size, size_t count,
                     void* user_param)
{
    const size_t buf_size = size * count;
    if (!user_param)
        return buf_size;

    std::vector<char> data(buf_size + 1);
    data[data.size() - 1] = '\0';
    memcpy(&data[0], buf, buf_size);

    // Discard if this is a separator.
    const char kSeperator[] = "\r\n";
    if (!memcmp(&data[0], kSeperator, sizeof(kSeperator)))
        return buf_size;

    std::vector<std::string>* data_pieces =
        reinterpret_cast<std::vector<std::string>*>(user_param);
    data_pieces->push_back(&data[0]);

    // cout << "Received piece: " << data_pieces->back() << endl;
    return buf_size;
}

std::string GetLocalDirectory()
{
    std::vector<char> buf(MAX_PATH);
    if (GetModuleFileNameA(nullptr, &buf[0], buf.size())) {
        std::string module_file(&buf[0]);
        int pos = module_file.find_last_of("\\/");
        if (pos != std::string::npos)
            return module_file.substr(0, pos + 1);
    }

    return std::string();
}

void LoadAccount(std::string* smtp_url, std::string* pop3_url, std::string* user,
                 std::string* password)
{
    std::string local_dir = GetLocalDirectory();
    if (local_dir.empty())
        return;

    std::string user_file_path = local_dir += "config\\user";
    std::ifstream user_file(user_file_path.c_str(), std::ios::in);
    user_file >> *smtp_url >> *pop3_url >> *user >> *password;
}

void LoadRecipient(std::string* recipient)
{
    std::string local_dir = GetLocalDirectory();
    if (local_dir.empty())
        return;

    std::string recipient_file_path = local_dir += "config\\recipient";
    std::ifstream recipient_file(recipient_file_path.c_str(), std::ios::in);
    recipient_file >> *recipient;
}

std::vector<std::string> GetIndices(const std::vector<std::string>& data_pieces)
{
    std::vector<std::string> result(data_pieces.size());
    for (auto i = data_pieces.begin(); i != data_pieces.end(); i++) {
        if (i->length() < 1)
            continue;

        result[std::distance(data_pieces.begin(), i)] = i->substr(0, 1);
    }

    return std::move(result);
}

void DumpMail(const std::string& index,
              const std::vector<std::string>& mail_lines)
{
    std::string dump_file_path = "e:\\mail_dump_" + index;
    std::ofstream dump_file(dump_file_path);
    for (auto i = mail_lines.begin(); i != mail_lines.end(); i++) {
        dump_file << *i << endl;
    }
}

void RetrieveMails(CURL* curl, const std::vector<std::string>& indices)
{
    // List all mails.
    for (auto i = indices.begin(); i != indices.end(); i++) {
        std::vector<std::string> mail_lines;
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &mail_lines);

        std::string command = "RETR " + *i;
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, command.c_str());
        curl_easy_perform(curl);
        // DumpMail(*i, mail_lines);
    }
}

void ReadMail()
{
    std::string smtp_url;
    std::string pop3_url;
    std::string user;
    std::string password;
    LoadAccount(&smtp_url, &pop3_url, &user, &password);
    pop3_url = "pop3://" + pop3_url;

    CURL* curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_USERNAME, user.c_str());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_str());
        curl_easy_setopt(curl, CURLOPT_URL, pop3_url.c_str());
        curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &WriteCallback);

        std::vector<std::string> data_pieces;
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data_pieces);

        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);

        CURLcode return_code = curl_easy_perform(curl);
        if (return_code == CURLE_OK) {
            RetrieveMails(curl, GetIndices(data_pieces));
        }

        curl_easy_cleanup(curl);
    }
}

std::string DayToString(WORD day)
{
    const char* days[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    return days[day];
}

std::string MonthToString(WORD month)
{
    const char* months[] = {
        "Jan", "Feb", "Mat", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct",
        "Nov", "Dec"
    };
    return months[month - 1];
}

std::string BuildSendingDate()
{
    SYSTEMTIME sys_time;
    ::GetLocalTime(&sys_time);

    std::stringstream ss;
    ss << "Date: " << DayToString(sys_time.wDayOfWeek) << ", "
        << sys_time.wDay << " " << MonthToString(sys_time.wMonth) << " "
        << sys_time.wYear << " " << sys_time.wHour << ":" << sys_time.wMinute
        << ":" << sys_time.wSecond << " +0800" << "\r\n";
    return ss.str();
}

std::string BuildSender(const std::string& sender)
{
    return "From: \"" + sender + "\" <" + sender + ">\r\n";
}

std::string BuildRecipient(const std::string& recipient)
{
    return "To: " + recipient + " <" + recipient + ">\r\n";
}

std::string BuildSubject(const std::string& subject)
{
    return "Subject: " + subject + "\r\n";
}

std::string BuildMessageID()
{
    SYSTEMTIME sys_time;
    ::GetLocalTime(&sys_time);

    std::stringstream ss;
    ss << sys_time.wYear << sys_time.wMonth << sys_time.wDay << sys_time.wHour
        << sys_time.wMinute << sys_time.wSecond << sys_time.wMilliseconds;
    return "Message-ID: <" + ss.str() + "@sina.com>\r\n";
}

std::string BuildSignature(const std::string& signature)
{
    return signature + "\r\n";
}

std::vector<std::string> BuildPayloadText(const std::string& sender,
                                          const std::string& recipient,
                                          const std::string& message)
{
    const std::string payload_text[] = {
        BuildSendingDate(),
        BuildSender(sender),
        BuildRecipient(recipient),
        BuildSubject(message),
        "X-Priority: 3\r\n",
        "X-Has-Attach: no\r\n",
        "X-Mailer: libcurl\r\n",
        "Mime-Version: 1.0\r\n",
        BuildMessageID(),
        "Content-Type: multipart/alternative;\r\n",
        "\tboundary=\"----=_2938sci3847afa8rqoi384273427426dk388_=----\"\r\n",
        "\r\n",
        "This is a multi-part message in MIME format.\r\n",
        "\r\n",
        "----=_2938sci3847afa8rqoi384273427426dk388_=----\r\n",
        "Content-Type: text/plain;\r\n",
        "\tcharset=\"GB2312\"\r\n",
        "Content-Transfer-Encoding: base64\r\n",
        "\r\n",
        Base64Encode(message) + "\r\n",
        "\r\n",
        "----=_2938sci3847afa8rqoi384273427426dk388_=----\r\n",
        "Content-Type: text/plain;\r\n",
        "\tcharset=\"GB2312\"\r\n",
        "Content-Transfer-Encoding: base64\r\n",
        "\r\n",
        "1eLKx9K7uPa3x7Oj1tjSqrXEz/vPog==\r\n",
        "\r\n",
//         "----=_2938sci3847afa8rqoi384273427426dk388_=----\r\n",
//         "Content-Type: text/html;\r\n",
//         "\tcharset=\"GB2312\"\r\n",
//         "Content-Transfer-Encoding: quoted-printable\r\n",
//         "\r\n",
//         "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0 Transitional//EN\">\r\n",
//         "<HTML><HEAD>\r\n",
//         "<META content=3D\"text/html; charset=3Dgb2312\" http-equiv=3DContent-Type>\r\n",
//         "<STYLE>\r\n",
//         "BLOCKQUOTE {\r\n",
//         ".MARGIN-BOTTOM: 0px; MARGIN-LEFT: 2em; MARGIN-TOP: 0px\r\n",
//         "}\r\n",
//         "OL {\r\n",
//         ".MARGIN-BOTTOM: 0px; MARGIN-TOP: 0px\r\n",
//         "}\r\n",
//         "UL {\r\n",
//         ".MARGIN-BOTTOM: 0px; MARGIN-TOP: 0px\r\n",
//         "}"
//         "P {\r\n",
//         ".MARGIN-BOTTOM: 0px; MARGIN-TOP: 0px\r\n",
//         "}\r\n",
//         "BODY {\r\n",
//         ".FONT-SIZE: 10.5pt; FONT-FAMILY: =CE=A2=C8=ED=D1=C5=BA=DA; COLOR: #000000;=\r\n",
//         " LINE-HEIGHT: 1.5\r\n",
//         "}\r\n",
//         "</STYLE>\r\n",
//         "\r\n",
//         "<META name=3DGENERATOR content=3D\"MSHTML 11.00.9600.16476\"></HEAD>\r\n",
//         "<BODY style=3D\"MARGIN: 10px\">\r\n",
//         "<DIV>sdiilei 9894ljjkj</DIV>\r\n",
//         "<DIV>&nbsp;</DIV>\r\n",
//         "<HR style=3D\"HEIGHT: 1px; WIDTH: 210px\" align=3Dleft color=3D#b5c4df SIZE=\r\n",
//         "=3D1>\r\n",
//         "\r\n",
//         "<DIV><SPAN>\r\n",
//         "<DIV style=3D\"FONT-SIZE: 10pt; FONT-FAMILY: verdana; MARGIN: 10px\">\r\n",
//         "<DIV>\r\n",
//         BuildSignature(sender),
//         "</DIV></DIV></SPAN></DIV></BODY></HTML>\r\n",
//         "\r\n",
        "----=_2938sci3847afa8rqoi384273427426dk388_=----\r\n",
        "\r\n.\r\n",
        ""
    };

    std::vector<std::string> result;
    int i = 0;
    while (!payload_text[i].empty())
    {
        cout << payload_text[i];
        result.push_back(std::move(payload_text[i]));
        i++;
    }
    result.push_back("");
    return std::move(result);
}

size_t ReadPayload(void* buf, size_t size, size_t nmemb, void* user_param)
{
    UploadInfo* upload_info = reinterpret_cast<UploadInfo*>(user_param);
    if ((size == 0) || (nmemb == 0) || ((size*nmemb) < 1)) {
        return 0;
    }

    const std::string& text =
        upload_info->upload_text[upload_info->current_index];
    if (!text.empty()) {
        memcpy(buf, &text[0], text.length());
        upload_info->current_index++;
        return text.length();
    }

    return 0;
}

void SendMail(const std::string& message)
{
    std::string smtp_url;
    std::string pop3_url;
    std::string user;
    std::string password;
    LoadAccount(&smtp_url, &pop3_url, &user, &password);
    smtp_url = "smtp://" + smtp_url;
    if (user.empty())
        return;

    std::string recipient;
    LoadRecipient(&recipient);
    if (recipient.empty())
        return;

    CURL* curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_USERNAME, user.c_str());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_str());
        curl_easy_setopt(curl, CURLOPT_URL, smtp_url.c_str());
        curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);

        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        // Set the mail sender.
        const std::string sender = "<" + user + ">";
        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, sender.c_str());

        // Set the recipients.
        curl_slist* recipients = nullptr;
        const std::string to = "<" + recipient + ">";
        recipients = curl_slist_append(recipients, to.c_str());
        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

        curl_easy_setopt(curl, CURLOPT_READFUNCTION, &ReadPayload);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);

        UploadInfo upload_info = {
            BuildPayloadText(user, recipient, message), 0
        };
        curl_easy_setopt(curl, CURLOPT_READDATA, &upload_info);

        // Some servers may need this information.
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

        CURLcode return_code = curl_easy_perform(curl);
        if (return_code == CURLE_OK) {
            
        }

        curl_slist_free_all(recipients);
        curl_easy_cleanup(curl);
    }
}

void DumpHtml(const std::vector<std::string>& html_lines)
{
    std::string dump_file_path = "e:\\html_dump";
    std::ofstream dump_file(dump_file_path);
    for (auto i = html_lines.begin(); i != html_lines.end(); i++) {
        dump_file << *i << endl;
    }
}

std::string GetPublicIPInIFrame(CURL* curl, const std::string& url)
{
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

    std::vector<std::string> data_pieces;
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data_pieces);

    CURLcode return_code = curl_easy_perform(curl);
    if (return_code == CURLE_OK) {
        for (auto i = data_pieces.begin(); i != data_pieces.end(); i++) {
            const std::string hint("ÄúµÄIPÊÇ");
            const int pos = i->find(hint.c_str());
            if (pos != i->npos) {
                const int start_pos = pos + hint.length() + 3;
                int end_pos = i->find(']', start_pos);
                if (end_pos != i->npos) {
                    return i->substr(start_pos, end_pos - start_pos);
                }
            }
        }
    }

    return "";
}

std::string GetPublicIP()
{
    CURL* curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "http://ip138.com");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &WriteCallback);

        std::vector<std::string> data_pieces;
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data_pieces);

        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);

        CURLcode return_code = curl_easy_perform(curl);
        if (return_code == CURLE_OK) {
            for (auto i = data_pieces.begin(); i != data_pieces.end(); i++) {
                const std::string hint("iframe src=\"");
                const int pos = i->find(hint.c_str());
                if (pos != i->npos) {
                    const int end_pos = i->find('\"', pos + hint.length() + 1);
                    if (end_pos != i->npos) {
                        return GetPublicIPInIFrame(
                            curl,
                            i->substr(pos + hint.length(),
                                      end_pos - hint.length() - pos));
                    }
                }
            }
            
            // DumpHtml(data_pieces);
        }
        
        curl_easy_cleanup(curl);
    }

    return "";
}

int main(int argc, wchar_t* argv[])
{
    cout << "Start working..." << endl;
    curl_global_init(CURL_GLOBAL_ALL);

    Dispatcher dispatcher;
    dispatcher.Start();

    bool quit = false;
    do {
        std::string command;
        cin >> command;
        quit = command.find("exit") != command.npos;
    } while (!quit);

    dispatcher.Stop();

    curl_global_cleanup();
    cin.ignore();
	return 0;
}