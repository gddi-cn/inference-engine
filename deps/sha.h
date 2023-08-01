#ifndef __SHA_H__
#define __SHA_H__

#include <vector>
#include <string>
#include <sys/types.h>
#include <fstream>
#include <string.h>
#include <openssl/sha.h>
#include <openssl/md5.h>
// #include <unistd.h>
#include <map>

namespace SHA
{
    static std::string hash256ToHex(const std::vector<uchar> &hash)
    {
        std::string buffer;
        buffer.resize(hash.size() * 2);
        for(int i = 0; i < hash.size(); i++)
        {
            sprintf((char *)buffer.data() + (i * 2), "%02x", hash[i]);
        }
        return buffer;
    }

    static std::vector<uchar> getTextHash256(const std::string &context)
    {
        auto hash = std::vector<uchar>(SHA256_DIGEST_LENGTH);
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, context.data(), context.size());
        SHA256_Final((uchar *)hash.data(), &sha256);
        return hash;
    }

    static std::string getTextHash256ToHex(const std::string &context)
    {
        auto hash = getTextHash256(context);
        std::string buffer;
        buffer.resize(SHA256_DIGEST_LENGTH * 2);
        for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        {
            sprintf((char *)buffer.data() + (i * 2), "%02x", hash[i]);
        }
        return buffer;
    }

    static std::vector<uchar> getFileHash256(const std::string &filePath)
    {
        FILE *fp = fopen(filePath.c_str(), "rb");
        if (!fp)
        {
            throw std::runtime_error("ERROR: failed to open file: " + filePath);
        }

        std::vector<char> buffer(8192);
        auto hash = std::vector<uchar>(SHA256_DIGEST_LENGTH);
        SHA256_CTX sha256;
        SHA256_Init(&sha256);

        int len = 0;
        while((len = fread(buffer.data(), 1, 8192, fp)) > 0)
        {
            SHA256_Update(&sha256, buffer.data(), len);
        }
        SHA256_Final((uchar *)hash.data(), &sha256);

        fclose(fp);

        return hash;
    }

    static std::string getFileHash256ToHex(const std::string &filePath)
    {
        auto hash = getFileHash256(filePath);
        std::string buffer;
        buffer.resize(SHA256_DIGEST_LENGTH * 2);
        for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)
        {
            sprintf((char *)buffer.data() + (i * 2), "%02x", hash[i]);
        }
        return buffer;
    }

    static std::vector<uchar> getFileMD5(const std::string &filePath)
    {
        FILE *fp = fopen(filePath.c_str(), "rb");
        if (!fp)
        {
            throw std::runtime_error("ERROR: failed to open file: " + filePath);
        }
        
        MD5_CTX ctx;
        MD5_Init(&ctx);
        std::vector<char> buffer(8192);
        auto md5 = std::vector<uchar>(MD5_DIGEST_LENGTH);

        int len = 0;
        while((len = fread(buffer.data(), 1, 8192, fp)) > 0)
        {
            MD5_Update(&ctx, buffer.data(), len);
        }
        MD5_Final((uchar *)md5.data(), &ctx);

        fclose(fp);

        return md5;
    }

    static std::string getFileMD5ToHex(const std::string &filePath)
    {
        auto md5 = getFileMD5(filePath);
        std::string md5_hex;
        md5_hex.resize(MD5_DIGEST_LENGTH * 2);
        for(int i = 0; i < MD5_DIGEST_LENGTH; i++)
        {
            sprintf((char *)md5_hex.data() + (i * 2), "%02x", md5[i]);
        }
        return md5_hex;
    }
};

#endif