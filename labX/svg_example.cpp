#include <SDL3/SDL.h>
#include <iostream>
#include <lunasvg.h>
#include <stdexcept>

using namespace lunasvg;

int main(int argc, char *argv[]) {
  // Check for SVG file argument
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <svg_file>" << std::endl;
    return 1;
  }

  // Initialize SDL3
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::cerr << "SDL init failed: " << SDL_GetError() << std::endl;
    return 1;
  }

  // Create window (800x600)
  SDL_Window *window = SDL_CreateWindow("LunaSVG + SDL3 Example", 800, 600, 0);
  if (!window) {
    std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
    SDL_Quit();
    return 1;
  }

  // Create renderer
  SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);
  if (!renderer) {
    std::cerr << "Renderer creation failed: " << SDL_GetError() << std::endl;
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  // Load and render SVG with LunaSVG
  auto document = Document::loadFromFile(argv[1]);
  if (!document) {
    std::cerr << "Failed to load SVG" << std::endl;
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  // Render to bitmap at desired size (e.g., 512x512)
  auto bitmap = document->renderToBitmap(512, 512);
  if (bitmap.isNull()) {
    std::cerr << "Failed to render bitmap" << std::endl;
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  // Create SDL surface from bitmap (BGRA format)
  SDL_Surface *surface = SDL_CreateSurfaceFrom(
      bitmap.width(), bitmap.height(), SDL_PIXELFORMAT_ABGR8888, bitmap.data(), bitmap.stride());
  if (!surface) {
    std::cerr << "Failed to create surface: " << SDL_GetError() << std::endl;
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  // Create texture from surface
  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  SDL_DestroySurface(surface);
  if (!texture) {
    std::cerr << "Failed to create texture: " << SDL_GetError() << std::endl;
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  // Main loop
  bool running = true;
  SDL_Event event;
  while (running) {
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        running = false;
      }
    }

    // Clear screen
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // Render texture (centered)
    SDL_FRect dst = {(800.0f - bitmap.width()) / 2, (600.0f - bitmap.height()) / 2,
                     static_cast<float>(bitmap.width()), static_cast<float>(bitmap.height())};
    SDL_RenderTexture(renderer, texture, nullptr, &dst);

    SDL_RenderPresent(renderer);
  }

  // Cleanup
  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}