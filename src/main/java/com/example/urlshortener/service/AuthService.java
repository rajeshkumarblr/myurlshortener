package com.example.urlshortener.service;

import com.example.urlshortener.dto.AuthResponse;
import com.example.urlshortener.dto.LoginRequest;
import com.example.urlshortener.dto.RegisterRequest;
import com.example.urlshortener.model.AppUser;
import com.example.urlshortener.repository.AppUserRepository;
import lombok.RequiredArgsConstructor;
import org.mindrot.jbcrypt.BCrypt;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

@Service
@RequiredArgsConstructor
public class AuthService {

    private final AppUserRepository userRepository;
    private final JwtService jwtService;

    @Transactional
    public AuthResponse register(RegisterRequest request) {
        String email = request.getEmail().toLowerCase();
        if (userRepository.findByEmail(email).isPresent()) {
            throw new IllegalArgumentException("Email already registered");
        }

        AppUser user = new AppUser();
        user.setName(request.getName());
        user.setEmail(email);
        user.setPasswordHash(BCrypt.hashpw(request.getPassword(), BCrypt.gensalt()));

        if ("admin@example.com".equalsIgnoreCase(email)) {
            user.setRole("ADMIN");
        }

        user = userRepository.save(user);

        String token = jwtService.generateToken(user.getId(), user.getEmail());
        return new AuthResponse(user.getId(), user.getName(), user.getEmail(), token, user.getRole());
    }

    public AuthResponse login(LoginRequest request) {
        String email = request.getEmail().toLowerCase();
        AppUser user = userRepository.findByEmail(email)
                .orElseThrow(() -> new IllegalArgumentException("Invalid email or password"));

        if (!BCrypt.checkpw(request.getPassword(), user.getPasswordHash())) {
            throw new IllegalArgumentException("Invalid email or password");
        }

        String token = jwtService.generateToken(user.getId(), user.getEmail());
        return new AuthResponse(user.getId(), user.getName(), user.getEmail(), token, user.getRole());
    }
    
    public AppUser getUser(Long id) {
        return userRepository.findById(id).orElse(null);
    }
}
