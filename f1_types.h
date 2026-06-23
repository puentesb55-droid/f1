#pragma once
#include <string>
#include <vector>
#include <array>
#include "raylib.h"
#include "raymath.h"

// ═══════════════════════════════════════════════════════════  
//  CONSTANTES GLOBALES (Macros y Literales de Compilación)
//  Explicación para tu exposición: Definen el entorno de simulación rígido,
//  estableciendo dimensiones espaciales físicas y la tasa de refresco del motor de físicas.
// ═══════════════════════════════════════════════════════════
constexpr int   SCREEN_W       = 1280;         // Ancho de la ventana de renderizado en píxeles
constexpr int   SCREEN_H       = 720;          // Alto de la ventana de renderizado en píxeles
constexpr int   MAX_AUTOS      = 8;            // Cota máxima de vehículos simultáneos en memoria (evita desbordamientos)
constexpr float TRACK_WIDTH    = 12.0f;        // Ancho genérico de la calzada en unidades métricas de simulación
constexpr float CAR_LENGTH     = 4.5f;         // Dimensión del chasis sobre el eje longitudinal (Z local)
constexpr float CAR_WIDTH      = 2.0f;         // Dimensión del chasis sobre el eje transversal (X local)
constexpr float CAR_HEIGHT     = 1.0f;         // Dimensión del chasis sobre el eje vertical (Y local)
constexpr float WHEEL_R        = 0.35f;        // Radio de colisión y renderizado de los neumáticos
constexpr float PHYSICS_DT     = 1.0f / 60.0f; // Diferencial de tiempo constante (Delta Time síncrono a 60Hz) para estabilidad física

// ═══════════════════════════════════════════════════════════
//  ESTRUCTURA DE ALMACENAMIENTO SOLAPADO: UNIÓN (Memory Overlay)
//  Explicación para tu exposición: Es una optimización de memoria. Como un auto sólo
//  puede equipar un motor a la vez, una estructura 'union' reserva únicamente el espacio 
//  del miembro más grande, compartiendo la misma dirección de memoria base.
// ═══════════════════════════════════════════════════════════
union EspecificacionesMotor {
    // Parámetros para unidades de potencia de la era híbrida moderna
    struct { float turboBoost; float recuperacion; } hibrido;  
    // Parámetros de la era clásica (motores atmosféricos de gran cubicaje y peso inercial)
    struct { float aspiracion; float peso; }          v12;     
    // Parámetros de enfoque de carga y eficiencia aerodinámica del chasis
    struct { float downforce;  float drag; }          aero;    
};

// ═══════════════════════════════════════════════════════════
//  ESTRUCTURA ENTIDAD AUTO (F1 Vehicle Entity Record)
//  Explicación para tu exposición: Objeto principal que consolida el estado 
//  cinemático, las variables de rendimiento dinámico del piloto, la telemetría y el buffer gráfico.
// ═══════════════════════════════════════════════════════════
struct Auto {
    // ── Datos de Identificación y Estética ──
    std::string  piloto;            // Nombre del competidor asignado
    std::string  equipo;            // Escudería para el agrupamiento lógico
    Color        colorCuerpo;       // Paleta RGB principal para el renderizado del chasis
    Color        colorAcento;       // Paleta RGB secundaria para alerones y detalles cosméticos
    char         inicial;           // Carácter nemotécnico utilizado en el minimapa / HUD

    // ── Variables Dinámicas y de Físicas ──
    float  velocidadBase;      // Velocidad punta nominal teórica en condiciones ideales (km/h)
    float  velocidadActual;    // Magnitud escalar de la velocidad lineal instantánea (km/h)
    float  aceleracion;        // Capacidad de cambio de velocidad positiva respecto al tiempo (m/s²)
    float  frenado;            // Capacidad de desaceleración por fricción mecánica (m/s²)
    float  agarre;             // Coeficiente de fricción estática base entre el neumático y el asfalto (0 a 1)
    float  destreza;           // Multiplicador estocástico que reduce el margen de error de la IA (0 a 1)
    float  anguloDireccion;    // Desviación angular de las ruedas directrices con respecto al vector tangente de la pista
    float  derrapeIntensidad;  // Métrica acumulativa del coeficiente de deslizamiento lateral por pérdida de tracción (0 a 1)
    float  derrapeFade;        // Constante de amortiguación para el restablecimiento de la adherencia lineal

    // ── Telemetría e Indicadores de Progreso en Pista ──
    float  distanciaRecorrida; // Posición escalar unidimensional proyectada sobre el spline de la pista
    int    vuelta;             // Contador de ciclos completos acumulados sobre la meta
    float  tiempoVuelta;       // Cronómetro instantáneo de la vuelta en curso (en segundos)
    float  mejorVuelta;        // Registro histórico de la vuelta más rápida (Personal Best)
    float  tiempoTotal;        // Tiempo total acumulado en carrera por la entidad
    int    posicion;           // Clasificación o ranking en tiempo real de la carrera
    int    posicionAnterior;   // Histórico inmediato de la posición para evaluar adelantamientos o pérdidas de puesto

    // ── Máquina de Estados de la Entidad ──
    bool   accidentado;        // Flag booleano que interrumpe la física ordinaria en caso de colisión severa
    int    turnosParado;       // Contador de frames de penalización durante el evento de recuperación (Recovery)
    bool   esJugador;          // Discriminador de control: true si recibe inputs de perifericos, false si delega en el script de IA
    int    nivelIA;            // Selector de comportamiento heurístico (0: Conservador / Lento -> 3: Agresivo / Óptimo)

    // ── Vectores Cinemáticos en el Espacio Tridimensional ──
    Vector3  pos3D;            // Coordenadas cartesianas (X, Y, Z) en el espacio global del mundo 3D
    Vector3  vel3D;            // Vector velocidad lineal de traslación tridimensional
    float    yaw;              // Ángulo de rotación sobre el eje vertical Y (Orientación de la dirección en radianes)
    float    roll;             // Ángulo de balanceo sobre el eje longitudinal Z (Inclinación por peralte o inercia de curva)
    float    pitch;            // Ángulo de cabeceo sobre el eje transversal X (Inclinación por aceleración/frenado o desniveles)

    // ── Atributos del Sistema de Propulsión ──
    char                 tipoMotor; // Discriminador para la unión de datos: 'H' (Híbrido), 'V' (V12), 'A' (Aero)
    EspecificacionesMotor motor;     // Estructura de unión de memoria declarada previamente

    // ── Subsistema de Renderizado y Gráficos ──
    Camera3D camPers;          // Estructura de cámara interna vinculada al chasis (para vistas subjetivas o de seguimiento)

    // Buffers dinámicos FIFO que almacenan las últimas coordenadas espaciales donde hubo pérdida de tracción para dibujar las marcas en el asfalto
    std::vector<Vector3> rastroIzq; // Nube de puntos de la banda de rodadura izquierda
    std::vector<Vector3> rastroDer; // Nube de puntos de la banda de rodadura derecha
};

// ═══════════════════════════════════════════════════════════
//  ESTRUCTURA NODO DE CONTROL DE PISTA (Spline Track Segment Node)
//  Explicación para tu exposición: El circuito no se modela de forma libre; se divide
//  en un conjunto encadenado de nodos discretos que contienen la geometría local y las reglas físicas del tramo.
// ═══════════════════════════════════════════════════════════
struct NodoPista {
    Vector3 centro;         // Punto geométrico central de la calzada en el espacio tridimensional
    Vector3 tangente;       // Vector unitario que apunta hacia la dirección de avance de la pista (Eje local de aceleración)
    Vector3 lateral;        // Vector unitario perpendicular a la trayectoria (Eje local para desplazamientos transversales)
    float   anchoLocal;     // Escala de separación de los bordes de la calzada en este nodo específico
    float   inclinacion;    // Ángulo de peralte o inclinación lateral de la pista en radianes
    float   limitVelocidad; // Restricción de velocidad segura (Top speed advisory) sugerida para la IA
    bool    esCurva;        // Flag lógico para activar algoritmos específicos de dirección y derrape
    float   radio;          // Radio de curvatura local (Tiende a infinito en rectas, valores pequeños implican giros cerrados)
    float   curvatura;      // Magnitud matemática inversa al radio ($1/R$). Define la severidad geométrica del tramo
};

// ═══════════════════════════════════════════════════════════
//  ESTRUCTURAS DE CONFIGURACIÓN Y CONTEXTO ENUMS (Global State Contexts)
// ═══════════════════════════════════════════════════════════

// Tipos enumerados en clase para controlar el estado ambiental de la carrera
enum class Clima { SOLEADO, NUBLADO, LLUVIA, TORMENTA };

// Identificadores únicos para los circuitos disponibles
enum class PistaID { MONACO, SILVERSTONE, SUZUKA };

// Contenedor principal de parámetros para la inicialización y persistencia de la sesión de carrera
struct ConfigCarrera {
    PistaID  pista;           // ID del circuito seleccionado
    Clima    clima;           // Estado del clima seleccionado
    int      vueltas;         // Cantidad de ciclos programados para finalizar la prueba
    int      numAutos;        // Número de competidores totales inscritos en la parrilla
    float    factorClima;     // Coeficiente multiplicador de fricción (ej. 1.0f Soleado, 0.6f Tormenta)
    float    visibilidad;     // Factor normalizado (0.0 a 1.0) que modifica el horizonte de previsión (Lookahead) de la IA
    std::string nombreClima;  // Cadena de texto descriptiva para interfaces de usuario (UI)
    std::vector<Auto> autos;  // Vector con la colección de entidades participantes instanciadas
};

// Máquina de Estados Principal del Software (Finite State Machine Patterns)
enum class GameState {
    MENU_PRINCIPAL,
    SELECCION_PISTA,
    SELECCION_PILOTOS,
    CARRERA,
    PAUSA,
    RESULTADOS
};

// Enumeración de los modos dinámicos del pipeline de cámaras del visualizador 3D
enum class VistaCamera { 
    TERCERA_PERSONA, // Cámara de seguimiento inmersivo desde la zaga del coche
    AEREA,           // Perspectiva cenital ortogonal / picado absoluto centrado en el grupo
    TRANSMISION,     // Cámaras estáticas dispuestas a lo largo del circuito (Estilo televisión)
    LIBRE            // Cámara desacoplada manipulada mediante periféricos por el usuario
};
