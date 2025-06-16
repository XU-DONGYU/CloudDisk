#pragma once
#include <string>

class CryptoUtil
{
public:
    static std::string generate_salt(int length = 64);
    static std::string hash_password(const std::string& password, const std::string& salt);
    static std::string generate_token(const std::string& username);
    static bool verify_token(const std::string& token,  std::string& username);
private:
    /* 禁止构造对象 */
    CryptoUtil() = delete;
};
