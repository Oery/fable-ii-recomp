// Minimal vkCreateInstance probe (diagnoses ICD negotiation).
#include <vulkan/vulkan.h>
#include <stdio.h>

int main(void) {
  VkApplicationInfo app = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "probe",
      .apiVersion = VK_API_VERSION_1_3,
  };
  VkInstanceCreateInfo ci = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &app,
  };
  VkInstance inst = 0;
  VkResult r = vkCreateInstance(&ci, 0, &inst);
  printf("vkCreateInstance: %d\n", (int)r);
  if (r == VK_SUCCESS) {
    uint32_t n = 0;
    vkEnumeratePhysicalDevices(inst, &n, 0);
    printf("physical devices: %u\n", n);
    vkDestroyInstance(inst, 0);
  }
  return r == VK_SUCCESS ? 0 : 1;
}
