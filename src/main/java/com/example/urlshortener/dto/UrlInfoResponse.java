package com.example.urlshortener.dto;

import com.fasterxml.jackson.annotation.JsonProperty;
import lombok.Data;

@Data
public class UrlInfoResponse {
    private String code;
    private String url;
    @JsonProperty("short")
    private String shortUrl;
    @JsonProperty("created_at")
    private Long createdAt;
    @JsonProperty("expires_at")
    private Long expiresAt;
    @JsonProperty("ttl_active")
    private Boolean ttlActive;
}
