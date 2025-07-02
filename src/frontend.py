import pygame
import paho.mqtt.client as mqtt
import json
import sys
import os
import math

# Configurações da tela
SCREEN_WIDTH = 1280
SCREEN_HEIGHT = 720
DRONE_SIZE = 40  # Tamanho da imagem do drone

# Configurações MQTT
MQTT_BROKER = "192.168.4.2"  # Ou o IP do broker, se estiver em outra máquina
MQTT_PORT = 1883
MQTT_TOPIC = "drone/+/to_antenna"  # Recebe mensagens de todos os drones

# Faixa de coordenadas esperadas (ajuste conforme seus dados reais)
MIN_LAT = -90.0
MAX_LAT = 90.0
MIN_LON = -180.0
MAX_LON = 180.0

# Variável global para armazenar a posição do drone
drone_position = {"lat": 0.0, "lon": 0.0, "drone_id": None, "angle": 0.0}

# Inicializa o PyGame
pygame.init()
screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT))
pygame.display.set_caption("Drone Position Tracker - Mapa")
font = pygame.font.SysFont(None, 28)

# Carregar imagens
def load_image(name, size=None):
    try:
        image = pygame.image.load(name)
        if size:
            image = pygame.transform.scale(image, size)
        return image.convert_alpha()
    except pygame.error as e:
        print(f"Erro ao carregar imagem: {e}")
        # Criar uma imagem substituta
        surf = pygame.Surface(size or (50, 50), pygame.SRCALPHA)
        pygame.draw.rect(surf, (255, 0, 0), (0, 0, *size), 2) if size else None
        return surf

# Tente carregar o mapa e o drone
try:
    map_image = pygame.image.load("mapa.png")
    map_image = pygame.transform.scale(map_image, (SCREEN_WIDTH, SCREEN_HEIGHT))
except:
    print("Erro ao carregar mapa.png. Criando mapa alternativo...")
    # Criar um mapa simples como fallback
    map_image = pygame.Surface((SCREEN_WIDTH, SCREEN_HEIGHT))
    map_image.fill((100, 200, 255))  # Céu azul
    pygame.draw.rect(map_image, (50, 180, 70), (0, SCREEN_HEIGHT*2//3, SCREEN_WIDTH, SCREEN_HEIGHT//3))  # Terra
    pygame.draw.rect(map_image, (80, 80, 80), (0, SCREEN_HEIGHT*3//4, SCREEN_WIDTH, 20))  # Estrada

try:
    drone_image = pygame.image.load("drone.png")
    drone_image = pygame.transform.scale(drone_image, (DRONE_SIZE, DRONE_SIZE))
except:
    print("Erro ao carregar drone.png. Criando drone alternativo...")
    # Criar um drone simples como fallback
    drone_image = pygame.Surface((DRONE_SIZE, DRONE_SIZE), pygame.SRCALPHA)
    pygame.draw.circle(drone_image, (30, 30, 150), (DRONE_SIZE//2, DRONE_SIZE//2), DRONE_SIZE//3)  # Corpo
    pygame.draw.rect(drone_image, (100, 100, 100), (DRONE_SIZE//4, 0, DRONE_SIZE//2, DRONE_SIZE//6))  # Hélices

# Callback quando uma mensagem MQTT é recebida
def on_message(client, userdata, msg):
    global drone_position
    try:
        payload = msg.payload.decode()
        data = json.loads(payload)
        
        # DEBUG: Mostrar mensagem recebida
        print(f"Mensagem recebida no tópico: {msg.topic}")
        print(f"Conteúdo: {data}")
        
        # Verificar se é uma mensagem de posição
        if data.get("tipo") == "posicao":
            # Extrai o ID do drone do tópico
            topic_parts = msg.topic.split('/')
            drone_id = topic_parts[1] if len(topic_parts) > 1 else "unknown"
            
            # Atualiza a posição global do drone
            drone_position = {
                "lat": data["dados"]["lat"],
                "lon": data["dados"]["lon"],
                "drone_id": drone_id,
                "angle": data["dados"]["angulo"]
            }
            
            print(f"Drone {drone_id} position updated: {drone_position}")
            
    except json.JSONDecodeError:
        print("Erro ao decodificar JSON")
    except KeyError as e:
        print(f"Chave faltando no JSON: {e}")
    except Exception as e:
        print(f"Erro inesperado: {e}")

# Função para mapear coordenadas para pixels na tela
def map_coords_to_screen(lat, lon):
    try:
        # Normaliza as coordenadas para o intervalo [0, 1]
        normalized_x = (lon - MIN_LON) / (MAX_LON - MIN_LON)
        normalized_y = 1.0 - ((lat - MIN_LAT) / (MAX_LAT - MIN_LAT))  # Invertido para Y
        
        # Converte para coordenadas de tela
        screen_x = int(normalized_x * SCREEN_WIDTH)
        screen_y = int(normalized_y * SCREEN_HEIGHT)
        
        return screen_x, screen_y
    except:
        # Retornar centro da tela em caso de erro
        return SCREEN_WIDTH // 2, SCREEN_HEIGHT // 2

# Rotacionar imagem do drone
def rotate_drone(image, angle):
    try:
        return pygame.transform.rotate(image, -angle)  # Negativo para compensar direção
    except:
        return image

# Configurar cliente MQTT
client = mqtt.Client()
client.on_message = on_message
client.connect(MQTT_BROKER, MQTT_PORT, 60)
client.subscribe(MQTT_TOPIC)
client.loop_start()

# Loop principal
running = True
clock = pygame.time.Clock()
last_position = None

while running:
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False
        elif event.type == pygame.KEYDOWN:
            if event.key == pygame.K_ESCAPE:
                running = False
    
    # Desenha o mapa de fundo
    screen.blit(map_image, (0, 0))
    
    # Desenha o drone se tivermos uma posição válida
    if drone_position["drone_id"]:
        screen_x, screen_y = map_coords_to_screen(
            drone_position["lat"], 
            drone_position["lon"]
        )
        
        # Rotaciona a imagem do drone com base no ângulo real
        rotated_drone = rotate_drone(drone_image, drone_position["angle"])
        drone_rect = rotated_drone.get_rect(center=(screen_x, screen_y))
        
        # Desenha o drone
        screen.blit(rotated_drone, drone_rect)
        
        # Mostra informações do drone
        info_text = f"Drone: {drone_position['drone_id']} | Lat: {drone_position['lat']:.6f}, Lon: {drone_position['lon']:.6f}"
        text_surface = font.render(info_text, True, (255, 255, 255))
        
        # Fundo para o texto
        text_bg = pygame.Rect(10, 10, text_surface.get_width() + 20, text_surface.get_height() + 10)
        pygame.draw.rect(screen, (0, 0, 0, 180), text_bg, border_radius=5)
        pygame.draw.rect(screen, (0, 100, 255), text_bg, 2, border_radius=5)
        
        screen.blit(text_surface, (20, 15))
        
        # Mostra coordenadas na tela
        coord_text = f"Posição: {screen_x}, {screen_y}"
        coord_surface = font.render(coord_text, True, (255, 255, 255))
        screen.blit(coord_surface, (20, 50))
        
        # Desenha trajetória
        if last_position:
            pygame.draw.line(screen, (255, 0, 0), last_position, (screen_x, screen_y), 2)
        last_position = (screen_x, screen_y)
    
    # Mostra mensagem se não houver dados do drone
    else:
        info_text = "Aguardando dados do drone..."
        text_surface = font.render(info_text, True, (255, 255, 255))
        text_bg = pygame.Rect(10, 10, text_surface.get_width() + 20, text_surface.get_height() + 10)
        pygame.draw.rect(screen, (180, 0, 0, 180), text_bg, border_radius=5)
        screen.blit(text_surface, (20, 15))
    
    # Atualiza a tela
    pygame.display.flip()
    clock.tick(30)  # Limita a 30 FPS

# Encerra
client.loop_stop()
pygame.quit()
sys.exit()