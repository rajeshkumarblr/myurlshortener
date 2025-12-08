package com.example.urlshortener.dto;

import lombok.AllArgsConstructor;
import lombok.Data;

@Data
@AllArgsConstructor
public class AuthResponse {
    private Long user_id;
    private String name;
    private String email;
    private String token;
    private String role;
}
