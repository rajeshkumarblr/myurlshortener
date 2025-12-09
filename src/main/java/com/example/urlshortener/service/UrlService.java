package com.example.urlshortener.service;

import com.example.urlshortener.dto.ShortenRequest;
import com.example.urlshortener.dto.ShortenResponse;
import com.example.urlshortener.dto.UrlInfoResponse;
import com.example.urlshortener.model.AppUser;
import com.example.urlshortener.model.UrlMapping;
import com.example.urlshortener.repository.ClickEventRepository;
import com.example.urlshortener.repository.UrlMappingRepository;
import lombok.RequiredArgsConstructor;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.time.Instant;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Random;
import java.util.concurrent.TimeUnit;
import java.util.stream.Collectors;

@Service
@RequiredArgsConstructor
public class UrlService {

    private final UrlMappingRepository urlRepository;
    private final ClickEventRepository clickEventRepository;
    private final StringRedisTemplate redisTemplate;

    @Value("${app.base-url}")
    private String baseUrl;

    private static final String BASE62 = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    private final Random random = new Random();

    private String generateCode() {
        StringBuilder sb = new StringBuilder(7);
        for (int i = 0; i < 7; i++) {
            sb.append(BASE62.charAt(random.nextInt(BASE62.length())));
        }
        return sb.toString();
    }

    @Transactional
    public ShortenResponse shorten(ShortenRequest request, AppUser user) {
        String code;
        int attempts = 0;
        do {
            code = generateCode();
            attempts++;
            if (attempts > 5) {
                throw new RuntimeException("Failed to generate unique code");
            }
        } while (urlRepository.existsById(code));

        UrlMapping mapping = new UrlMapping();
        mapping.setCode(code);
        mapping.setUrl(request.getUrl());
        mapping.setUser(user);

        if (request.getTtl() != null && request.getTtl() > 0) {
            mapping.setExpiresAt(Instant.now().plusSeconds(request.getTtl()));
        }

        urlRepository.save(mapping);

        // Cache in Redis
        long ttl = (request.getTtl() != null && request.getTtl() > 0) ? request.getTtl() : 86400;
        redisTemplate.opsForValue().set(code, request.getUrl(), ttl, TimeUnit.SECONDS);

        return new ShortenResponse(code, baseUrl + "/" + code);
    }

    public Optional<String> resolve(String code) {
        // Try Redis
        String cachedUrl = redisTemplate.opsForValue().get(code);
        if (cachedUrl != null) {
            return Optional.of(cachedUrl);
        }

        // Try DB
        Optional<UrlMapping> mappingOpt = urlRepository.findById(code);
        if (mappingOpt.isPresent()) {
            UrlMapping mapping = mappingOpt.get();
            if (mapping.getExpiresAt() != null && mapping.getExpiresAt().isBefore(Instant.now())) {
                urlRepository.delete(mapping);
                return Optional.empty();
            }
            
            // Cache back to Redis (short TTL for resolved items if not specified, or remaining TTL)
            long ttl = 300; // 5 minutes default cache for resolved items
            if (mapping.getExpiresAt() != null) {
                long remaining = mapping.getExpiresAt().getEpochSecond() - Instant.now().getEpochSecond();
                if (remaining > 0) {
                    ttl = Math.min(remaining, 86400); // Cap at 1 day or remaining
                } else {
                    return Optional.empty(); // Expired
                }
            }
            redisTemplate.opsForValue().set(code, mapping.getUrl(), ttl, TimeUnit.SECONDS);
            
            return Optional.of(mapping.getUrl());
        }
        return Optional.empty();
    }

    public List<UrlInfoResponse> listUrls(AppUser user) {
        List<UrlMapping> mappings = urlRepository.findByUserIdOrderByCreatedAtDesc(user.getId());
        
        Map<String, Long> clicksMap = clickEventRepository.countClicksByUserId(user.getId()).stream()
                .collect(Collectors.toMap(obj -> (String) obj[0], obj -> (Long) obj[1]));

        return mappings.stream()
                .map(mapping -> {
                    UrlInfoResponse info = toInfoResponse(mapping);
                    info.setClicks(clicksMap.getOrDefault(mapping.getCode(), 0L));
                    return info;
                })
                .collect(Collectors.toList());
    }

    @Transactional
    public void deleteUrl(String code, AppUser user) {
        UrlMapping mapping = urlRepository.findById(code)
                .orElseThrow(() -> new RuntimeException("URL not found"));

        if (!mapping.getUser().getId().equals(user.getId())) {
            throw new RuntimeException("Unauthorized");
        }

        clickEventRepository.deleteByUrlMappingCode(code);
        urlRepository.delete(mapping);
        redisTemplate.delete(code);
    }

    public Optional<UrlInfoResponse> getInfo(String code) {
        return urlRepository.findById(code).map(this::toInfoResponse);
    }

    private UrlInfoResponse toInfoResponse(UrlMapping mapping) {
        UrlInfoResponse info = new UrlInfoResponse();
        info.setCode(mapping.getCode());
        info.setUrl(mapping.getUrl());
        info.setShortUrl(baseUrl + "/" + mapping.getCode());
        info.setCreatedAt(mapping.getCreatedAt().getEpochSecond());
        if (mapping.getExpiresAt() != null) {
            info.setExpiresAt(mapping.getExpiresAt().getEpochSecond());
            info.setTtlActive(mapping.getExpiresAt().isAfter(Instant.now()));
        } else {
            info.setTtlActive(true);
        }
        return info;
    }
}
