variable "resource_group_name" {
  default = "rg-urlshortener-wus3"
}

variable "location" {
  default = "westus3"
}

variable "acr_name" {
  default = "acrurlshortener2261"
}

variable "aks_name" {
  default = "aks-urlshortener"
}

variable "node_count" {
  default = 1
}

variable "node_size" {
  default = "Standard_B2ms"
}

variable "ssh_public_key" {
  description = "SSH Public Key for AKS nodes"
  type        = string
  sensitive   = true
}

variable "dns_label" {
  description = "DNS label for the Ingress Controller Public IP"
  default     = "urlshortener-rajesh"
}
