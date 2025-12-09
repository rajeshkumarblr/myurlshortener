package com.example.urlshortener.repository;

import com.example.urlshortener.model.ClickEvent;
import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Query;
import org.springframework.stereotype.Repository;

import java.util.List;

@Repository
public interface ClickEventRepository extends JpaRepository<ClickEvent, Long> {
    
    @Query("SELECT c.urlMapping.code, COUNT(c) FROM ClickEvent c GROUP BY c.urlMapping.code")
    List<Object[]> countClicksByUrl();

    @Query("SELECT c.urlMapping.code, COUNT(c) FROM ClickEvent c WHERE c.urlMapping.user.id = :userId GROUP BY c.urlMapping.code")
    List<Object[]> countClicksByUserId(Long userId);
    
    void deleteByUrlMappingCode(String code);

    long count();
}
