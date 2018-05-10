#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>

#include <windows.h>

#include "curl/curl.h"
#include "cryptopp/aes.h"
#include "cryptopp/base64.h"
#include "cryptopp/filters.h"
#include "cryptopp/modes.h"
#include "cryptopp/secblock.h"
#include "dispatcher.h"

using std::cin;
using std::cout;
using std::endl;

struct UploadInfo
{
    std::vector<std::string> upload_text;
    int current_index;
};

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

        const int pos = i->find(' ');
        if (pos != i->npos)
            result[std::distance(data_pieces.begin(), i)] = i->substr(0, pos);
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
    const int pos = recipient.find('@');
    const std::string name =
        pos != recipient.npos ? recipient.substr(0, pos) : recipient;
    return "To: " + name + " <" + recipient + ">\r\n";
}

std::string BuildSubject(const std::string& subject)
{
    return "Subject: " + subject + "\r\n";
}

bool LoadKey(CryptoPP::SecByteBlock* key_block)
{
    assert(key_block);
    if (!key_block)
        return false;

    std::string local_dir = GetLocalDirectory();
    if (local_dir.empty())
        return false;

    std::string key_file_path = local_dir += "config\\key";
    std::ifstream key_file(key_file_path.c_str(), std::ios::binary);
    if (key_file.bad() || key_file.fail())
        return false;
    
    key_file.seekg(0, std::ios::end);
    const int file_size = static_cast<int>(key_file.tellg());
    key_file.seekg(0);

    // The source data must be initialized to 0.
    memset(key_block->BytePtr(), 0, key_block->size());

    CryptoPP::SecByteBlock buf(*key_block);
    byte* data_ptr = buf.BytePtr();
    int data_remaining = file_size;
    while (data_remaining >= static_cast<int>(buf.size())) {
        key_file.read(reinterpret_cast<char*>(data_ptr), buf.size());
        byte* temp_data = key_block->BytePtr();
        for (int i = 0; i < static_cast<int>(buf.size()); i++)
            temp_data[i] = data_ptr[i] ^ temp_data[i];

        data_remaining -= buf.size();
    }

    if (data_remaining > 0) {
        key_file.read(reinterpret_cast<char*>(data_ptr), data_remaining);
        byte* temp_data = key_block->BytePtr();
        for (int i = 0; i < data_remaining; i++)
            temp_data[i] = data_ptr[i] ^ temp_data[i];
    }

    return true;
}

// Build a random iv to make the encrypted text lines look more different with
// repeated sub-strings.
std::vector<byte> BuildIV()
{
    // We will expand the buffer length to a multiply of |kBase64ByteBase| so
    // that to avoid "=" appearing at the back of IV text. Otherwise, the "="
    // could tell the readers that "this is a separator of two sections".
    //
    // When we decode it, just simply discard the redundant characters.
    const int kBase64ByteBase = 3;
    int buf_size = ((CryptoPP::AES::BLOCKSIZE + kBase64ByteBase - 1)
        / kBase64ByteBase) * kBase64ByteBase;

    srand(static_cast<unsigned int>(time(0)));
    std::vector<byte> result(buf_size);
    for (auto i = result.begin(); i != result.end(); i++)
        *i = rand() % 255;

    return std::move(result);
}

std::string EncryptMessage(const std::string& message)
{
    CryptoPP::SecByteBlock key(0, CryptoPP::AES::DEFAULT_KEYLENGTH);
    if (!LoadKey(&key))
        return std::string();

    std::vector<byte> iv = BuildIV();

    std::string cipher;
    try {
        CryptoPP::StringSink* string_sink(new CryptoPP::StringSink(cipher));
        CryptoPP::Base64Encoder* b64_enc(
            new CryptoPP::Base64Encoder(string_sink));
        CryptoPP::CBC_Mode<CryptoPP::AES>::Encryption aes(key, key.size(),
                                                          &iv[0]);
        CryptoPP::StreamTransformationFilter* trans(
            new CryptoPP::StreamTransformationFilter(aes, b64_enc));
        CryptoPP::StringSource source(message, true, trans);

        // Encode iv with Base64 encoder and catenate to the cipher.
        std::string iv_b64;
        CryptoPP::StringSink* iv_str_sink(new CryptoPP::StringSink(iv_b64));
        CryptoPP::Base64Encoder* b64_enc2(
            new CryptoPP::Base64Encoder(
                iv_str_sink, false/* We don't need '\n' at the back. */));
        CryptoPP::ArraySource source2(&iv[0], iv.size(), true, b64_enc2);

        cipher = iv_b64 + cipher;
    } catch (const CryptoPP::Exception& e) {
        cout << e.what() << endl;
    }

    return cipher;
}

std::string BuildMessageID(const std::string& sender)
{
    SYSTEMTIME sys_time;
    ::GetLocalTime(&sys_time);

    // Extract the domain name of the sender.
    const int pos = sender.find('@');
    const std::string domain_name =
        pos != sender.npos ? sender.substr(pos, -1) : "";

    std::stringstream ss;
    ss << sys_time.wYear << sys_time.wMonth << sys_time.wDay << sys_time.wHour
        << sys_time.wMinute << sys_time.wSecond << sys_time.wMilliseconds;
    return "Message-ID: <" + ss.str() + domain_name + ">\r\n";
}

std::string BuildSignature(const std::string& signature)
{
    return signature + "\r\n";
}

std::vector<std::string> BuildPayloadText(const std::string& sender,
                                          const std::string& recipient,
                                          const std::string& subject,
                                          const std::string& message)
{
    const std::string payload_text[] = {
        BuildSendingDate(),
        BuildSender(sender),
        BuildRecipient(recipient),
        BuildSubject(subject),
        "X-Priority: 3\r\n",
        "X-Has-Attach: no\r\n",
        "X-Mailer: libcurl\r\n",
        "Mime-Version: 1.0\r\n",
        BuildMessageID(sender),

        "Content-Type: text/plain; charset=UTF-8; format=flowed\r\n",
        "Content-Transfer-Encoding: 7bit\r\n",
        "\r\n",
        EncryptMessage(message),
        "\r\n",

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
    if ((size == 0) || (nmemb == 0) || ((size * nmemb) < 1)) {
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

void SendMail(const std::string& subject, const std::string& message)
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
            BuildPayloadText(user, recipient, subject, message), 0
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