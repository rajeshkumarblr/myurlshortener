#include "JwtService.h"
#include <json/json.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <memory>
#include <stdexcept>

namespace {
std::string compactJson(const Json::Value& value) {
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    return Json::writeString(builder, value);
}

std::chrono::system_clock::time_point fromEpochSeconds(long long seconds) {
    return std::chrono::system_clock::time_point{std::chrono::seconds{seconds}};
}
}  // namespace

JwtService::JwtService(std::string secret, std::chrono::seconds ttl)
    : secret_(std::move(secret)), ttl_(ttl) {
    if (secret_.empty()) {
        throw std::runtime_error("JWT secret must not be empty");
    }
    if (ttl_.count() <= 0) {
        throw std::runtime_error("JWT TTL must be positive");
    }
}

std::string JwtService::issueToken(long userId,
                                   std::string_view name,
                                   std::string_view email) const {
    Json::Value header;
    header["alg"] = "HS256";
    header["typ"] = "JWT";

    const auto now = std::chrono::system_clock::now();
    const auto expires = now + ttl_;
    const auto nowEpoch = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    const auto expEpoch = std::chrono::duration_cast<std::chrono::seconds>(expires.time_since_epoch()).count();

    Json::Value payload;
    payload["sub"] = static_cast<Json::Int64>(userId);
    payload["name"] = std::string(name);
    payload["email"] = std::string(email);
    payload["iat"] = static_cast<Json::Int64>(nowEpoch);
    payload["exp"] = static_cast<Json::Int64>(expEpoch);

    const auto encodedHeader = base64UrlEncode(compactJson(header));
    const auto encodedPayload = base64UrlEncode(compactJson(payload));
    const auto signingInput = encodedHeader + "." + encodedPayload;
    const auto signature = base64UrlEncode(hmacSha256(signingInput));

    return signingInput + "." + signature;
}

std::optional<JwtService::Claims> JwtService::verify(const std::string& token) const {
    auto firstDot = token.find('.');
    if (firstDot == std::string::npos) {
        return std::nullopt;
    }
    auto secondDot = token.find('.', firstDot + 1);
    if (secondDot == std::string::npos) {
        return std::nullopt;
    }

    const auto headerPart = token.substr(0, firstDot);
    const auto payloadPart = token.substr(firstDot + 1, secondDot - firstDot - 1);
    const auto signaturePart = token.substr(secondDot + 1);

    const auto headerRaw = base64UrlDecode(headerPart);
    const auto payloadRaw = base64UrlDecode(payloadPart);
    const auto signatureRaw = base64UrlDecode(signaturePart);
    if (!headerRaw || !payloadRaw || !signatureRaw) {
        return std::nullopt;
    }

    Json::Value headerJson;
    Json::Value payloadJson;
    Json::CharReaderBuilder builder;
    const auto parseJson = [&builder](const std::string& raw, Json::Value& target) {
        std::string errors;
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        return reader->parse(raw.data(), raw.data() + raw.size(), &target, &errors);
    };
    if (!parseJson(*headerRaw, headerJson) || !parseJson(*payloadRaw, payloadJson)) {
        return std::nullopt;
    }

    if (!headerJson.isMember("alg") || headerJson["alg"].asString() != "HS256") {
        return std::nullopt;
    }

    const auto signingInput = headerPart + "." + payloadPart;
    const auto expectedSignature = hmacSha256(signingInput);
    const auto& providedSignature = *signatureRaw;

    if (expectedSignature.size() != providedSignature.size()) {
        return std::nullopt;
    }
    unsigned char diff = 0;
    for (size_t i = 0; i < expectedSignature.size(); ++i) {
        diff |= static_cast<unsigned char>(expectedSignature[i] ^ providedSignature[i]);
    }
    if (diff != 0) {
        return std::nullopt;
    }

    if (!payloadJson.isMember("sub") || !payloadJson.isMember("exp")) {
        return std::nullopt;
    }

    const auto expEpoch = payloadJson["exp"].asInt64();
    const auto nowEpoch = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    if (expEpoch <= nowEpoch) {
        return std::nullopt;
    }

    Claims claims;
    claims.userId = payloadJson["sub"].asInt64();
    claims.name = payloadJson.get("name", "").asString();
    claims.email = payloadJson.get("email", "").asString();
    claims.expiresAt = fromEpochSeconds(expEpoch);
    return claims;
}

std::string JwtService::hmacSha256(const std::string& data) const {
    unsigned int len = 0;
    unsigned char result[EVP_MAX_MD_SIZE];
    auto digest = HMAC(EVP_sha256(),
                       secret_.data(),
                       static_cast<int>(secret_.size()),
                       reinterpret_cast<const unsigned char*>(data.data()),
                       data.size(),
                       result,
                       &len);
    if (digest == nullptr) {
        throw std::runtime_error("failed to compute HMAC");
    }
    return std::string(reinterpret_cast<char*>(result), len);
}

std::string JwtService::base64UrlEncode(const std::string& data) {
    BIO* base64 = BIO_new(BIO_f_base64());
    if (!base64) {
        throw std::runtime_error("failed to allocate base64 BIO");
    }
    BIO* memory = BIO_new(BIO_s_mem());
    if (!memory) {
        BIO_free(base64);
        throw std::runtime_error("failed to allocate memory BIO");
    }
    BIO_set_flags(base64, BIO_FLAGS_BASE64_NO_NL);
    BIO* bio = BIO_push(base64, memory);
    BIO_write(bio, data.data(), static_cast<int>(data.size()));
    BIO_flush(bio);
    BUF_MEM* bufferPtr = nullptr;
    BIO_get_mem_ptr(bio, &bufferPtr);
    std::string encoded(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);

    for (char& c : encoded) {
        if (c == '+') {
            c = '-';
        } else if (c == '/') {
            c = '_';
        }
    }
    while (!encoded.empty() && encoded.back() == '=') {
        encoded.pop_back();
    }
    return encoded;
}

std::optional<std::string> JwtService::base64UrlDecode(const std::string& input) {
    std::string data = input;
    for (char& c : data) {
        if (c == '-') {
            c = '+';
        } else if (c == '_') {
            c = '/';
        }
    }
    while (data.size() % 4 != 0) {
        data.push_back('=');
    }

    BIO* base64 = BIO_new(BIO_f_base64());
    if (!base64) {
        return std::nullopt;
    }
    BIO* source = BIO_new_mem_buf(data.data(), static_cast<int>(data.size()));
    if (!source) {
        BIO_free(base64);
        return std::nullopt;
    }
    BIO_set_flags(base64, BIO_FLAGS_BASE64_NO_NL);
    BIO* bio = BIO_push(base64, source);
    std::string decoded(data.size(), '\0');
    const int len = BIO_read(bio, decoded.data(), static_cast<int>(decoded.size()));
    BIO_free_all(bio);
    if (len <= 0) {
        return std::nullopt;
    }
    decoded.resize(static_cast<size_t>(len));
    return decoded;
}
