package com.example.urlshortener.config;

import com.example.urlshortener.service.AuthService;
import com.example.urlshortener.service.JwtService;
import io.jsonwebtoken.Claims;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import lombok.RequiredArgsConstructor;
import org.springframework.stereotype.Component;
import org.springframework.web.servlet.HandlerInterceptor;

@Component
@RequiredArgsConstructor
public class AuthInterceptor implements HandlerInterceptor {

    private final JwtService jwtService;
    private final AuthService authService;

    @Override
    public boolean preHandle(HttpServletRequest request, HttpServletResponse response, Object handler) throws Exception {
        if (request.getMethod().equals("OPTIONS")) return true;

        String authHeader = request.getHeader("Authorization");
        if (authHeader != null && authHeader.startsWith("Bearer ")) {
            String token = authHeader.substring(7);
            try {
                Claims claims = jwtService.validateToken(token);
                Long userId = claims.get("uid", Long.class);
                request.setAttribute("userId", userId);
                return true;
            } catch (Exception e) {
                // Invalid token
            }
        }
        
        // Allow public endpoints to pass through, but they won't have userId
        // The controller will check if userId is required
        return true;
    }
}
