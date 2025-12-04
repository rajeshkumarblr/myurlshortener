package com.example.urlshortener.controller;

import org.springframework.stereotype.Controller;
import org.springframework.web.bind.annotation.GetMapping;

@Controller
public class FrontendController {

    @GetMapping("/public/index.html")
    public String redirectOldPath() {
        return "redirect:/index.html";
    }
}
