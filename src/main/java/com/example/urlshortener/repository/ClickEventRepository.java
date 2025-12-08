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
    
    long count();
}
