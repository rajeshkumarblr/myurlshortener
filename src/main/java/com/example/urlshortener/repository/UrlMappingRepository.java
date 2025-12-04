package com.example.urlshortener.repository;

import com.example.urlshortener.model.UrlMapping;
import org.springframework.data.jpa.repository.JpaRepository;
import java.util.List;

public interface UrlMappingRepository extends JpaRepository<UrlMapping, String> {
    List<UrlMapping> findByUserIdOrderByCreatedAtDesc(Long userId);
}
