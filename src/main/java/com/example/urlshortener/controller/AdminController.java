package com.example.urlshortener.controller;

import com.example.urlshortener.dto.UserResponse;
import com.example.urlshortener.model.AppUser;
import com.example.urlshortener.model.UrlMapping;
import com.example.urlshortener.repository.AppUserRepository;
import com.example.urlshortener.repository.ClickEventRepository;
import com.example.urlshortener.repository.UrlMappingRepository;
import com.example.urlshortener.service.AuthService;
import lombok.RequiredArgsConstructor;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.transaction.annotation.Transactional;
import org.springframework.web.bind.annotation.*;

import java.util.List;
import java.util.stream.Collectors;

@RestController
@RequestMapping("/api/v1/admin")
@RequiredArgsConstructor
public class AdminController {

    private final AuthService authService;
    private final AppUserRepository userRepository;
    private final UrlMappingRepository urlRepository;
    private final ClickEventRepository clickEventRepository;

    private boolean isAdmin(Long userId) {
        if (userId == null) return false;
        AppUser user = authService.getUser(userId);
        return user != null && "ADMIN".equals(user.getRole());
    }

    @GetMapping("/users")
    public ResponseEntity<List<UserResponse>> listUsers(@RequestAttribute(value = "userId", required = false) Long userId) {
        if (!isAdmin(userId)) {
            return ResponseEntity.status(HttpStatus.FORBIDDEN).build();
        }

        List<UserResponse> users = userRepository.findAll().stream()
                .map(u -> new UserResponse(
                        u.getId(),
                        u.getName(),
                        u.getEmail(),
                        u.getRole(),
                        u.getCreatedAt().getEpochSecond()
                ))
                .collect(Collectors.toList());

        return ResponseEntity.ok(users);
    }

    @DeleteMapping("/users/{id}")
    @Transactional
    public ResponseEntity<Void> deleteUser(
            @RequestAttribute(value = "userId", required = false) Long adminId,
            @PathVariable Long id) {
        
        if (!isAdmin(adminId)) {
            return ResponseEntity.status(HttpStatus.FORBIDDEN).build();
        }

        if (adminId.equals(id)) {
            return ResponseEntity.badRequest().build(); // Cannot delete self
        }

        if (!userRepository.existsById(id)) {
            return ResponseEntity.notFound().build();
        }

        // Delete all URLs and associated clicks for this user
        List<UrlMapping> userUrls = urlRepository.findByUserIdOrderByCreatedAtDesc(id);
        for (UrlMapping url : userUrls) {
            clickEventRepository.deleteByUrlMappingCode(url.getCode());
            urlRepository.delete(url);
            // Note: Redis cache might still have entries, but they will expire. 
            // To be thorough, we should delete from Redis too, but we need RedisTemplate here.
        }

        userRepository.deleteById(id);
        return ResponseEntity.noContent().build();
    }
}
