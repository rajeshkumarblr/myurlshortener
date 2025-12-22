package com.example.urlshortener.monitoring;

import com.example.urlshortener.repository.AppUserRepository;
import com.example.urlshortener.repository.UrlMappingRepository;
import io.micrometer.core.instrument.Gauge;
import io.micrometer.core.instrument.MeterRegistry;
import org.springframework.stereotype.Component;

@Component
public class CustomMetrics {

    public CustomMetrics(MeterRegistry registry, 
                         AppUserRepository userRepository,
                         UrlMappingRepository urlRepository) {
        
        Gauge.builder("app.users.total", userRepository::count)
             .description("Total number of registered users")
             .register(registry);

        Gauge.builder("app.urls.total", urlRepository::count)
             .description("Total number of shortened URLs")
             .register(registry);
    }
}
