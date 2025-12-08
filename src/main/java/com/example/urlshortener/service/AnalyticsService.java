package com.example.urlshortener.service;

import com.example.urlshortener.model.ClickEvent;
import com.example.urlshortener.model.UrlMapping;
import com.example.urlshortener.repository.ClickEventRepository;
import com.example.urlshortener.repository.UrlMappingRepository;
import jakarta.servlet.http.HttpServletRequest;
import lombok.RequiredArgsConstructor;
import org.springframework.scheduling.annotation.Async;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.util.Optional;

@Service
@RequiredArgsConstructor
public class AnalyticsService {

    private final ClickEventRepository clickEventRepository;
    private final UrlMappingRepository urlMappingRepository;

    @Async
    @Transactional
    public void logClick(String code, HttpServletRequest request) {
        Optional<UrlMapping> mappingOpt = urlMappingRepository.findById(code);
        if (mappingOpt.isPresent()) {
            ClickEvent event = new ClickEvent();
            event.setUrlMapping(mappingOpt.get());
            event.setIpAddress(getClientIp(request));
            event.setUserAgent(request.getHeader("User-Agent"));
            event.setReferer(request.getHeader("Referer"));
            clickEventRepository.save(event);
        }
    }

    private String getClientIp(HttpServletRequest request) {
        String xForwardedFor = request.getHeader("X-Forwarded-For");
        if (xForwardedFor != null && !xForwardedFor.isEmpty()) {
            return xForwardedFor.split(",")[0].trim();
        }
        return request.getRemoteAddr();
    }

    public java.util.List<com.example.urlshortener.dto.AnalyticsResponse> getTopUrls() {
        return clickEventRepository.countClicksByUrl().stream()
                .map(obj -> new com.example.urlshortener.dto.AnalyticsResponse((String) obj[0], (Long) obj[1]))
                .collect(java.util.stream.Collectors.toList());
    }
    
    public long getTotalClicks() {
        return clickEventRepository.count();
    }
}
