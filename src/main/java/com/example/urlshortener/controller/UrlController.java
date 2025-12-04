package com.example.urlshortener.controller;

import com.example.urlshortener.dto.ShortenRequest;
import com.example.urlshortener.dto.ShortenResponse;
import com.example.urlshortener.dto.UrlInfoResponse;
import com.example.urlshortener.model.AppUser;
import com.example.urlshortener.service.AuthService;
import com.example.urlshortener.service.UrlService;
import jakarta.servlet.http.HttpServletResponse;
import jakarta.validation.Valid;
import lombok.RequiredArgsConstructor;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.io.IOException;
import java.util.List;
import java.util.Optional;

@RestController
@RequiredArgsConstructor
public class UrlController {

    private final UrlService urlService;
    private final AuthService authService;

    @GetMapping("/api/v1/health")
    public ResponseEntity<String> health() {
        // In a real app, check DB connection here
        return ResponseEntity.ok("ok");
    }

    @PostMapping("/api/v1/shorten")
    public ResponseEntity<ShortenResponse> shorten(
            @RequestAttribute(value = "userId", required = false) Long userId,
            @Valid @RequestBody ShortenRequest request) {
        
        if (userId == null) {
            return ResponseEntity.status(HttpStatus.UNAUTHORIZED).build();
        }

        AppUser user = authService.getUser(userId);
        if (user == null) {
            return ResponseEntity.status(HttpStatus.UNAUTHORIZED).build();
        }

        return ResponseEntity.ok(urlService.shorten(request, user));
    }

    @GetMapping("/api/v1/urls")
    public ResponseEntity<List<UrlInfoResponse>> listUrls(
            @RequestAttribute(value = "userId", required = false) Long userId) {
        
        if (userId == null) {
            return ResponseEntity.status(HttpStatus.UNAUTHORIZED).build();
        }

        AppUser user = authService.getUser(userId);
        if (user == null) {
            return ResponseEntity.status(HttpStatus.UNAUTHORIZED).build();
        }

        return ResponseEntity.ok(urlService.listUrls(user));
    }

    @GetMapping("/api/v1/info/{code}")
    public ResponseEntity<UrlInfoResponse> getInfo(@PathVariable String code) {
        return urlService.getInfo(code)
                .map(ResponseEntity::ok)
                .orElse(ResponseEntity.notFound().build());
    }

    @GetMapping("/{code}")
    public void redirect(@PathVariable String code, HttpServletResponse response) throws IOException {
        Optional<String> url = urlService.resolve(code);
        if (url.isPresent()) {
            response.sendRedirect(url.get());
        } else {
            response.sendError(HttpServletResponse.SC_NOT_FOUND);
        }
    }
}
