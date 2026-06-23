#pragma once
#include "f1_types.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

// ═══════════════════════════════════════════════════════════
//  DATOS DE PILOTOS (Catálogo maestro estático)
// ═══════════════════════════════════════════════════════════
// Esta estructura define la plantilla de estadísticas de rendimiento y aspecto visual
// de cualquier piloto disponible en el simulador.
struct DatoPiloto {
    std::string nombre;         // Nombre del piloto (ej: "Max Verstappen")
    std::string equipo;         // Nombre de la escudería (ej: "Red Bull Racing")
    float velBase;              // Velocidad punta máxima en km/h
    float aceleracion;          // Capacidad de respuesta del motor al arrancar o salir de curvas
    float frenado;              // Potencia y eficiencia de los frenos de carbono
    float agarre;               // Factor base de adherencia de los neumáticos al asfalto (0.0 a 1.0)
    float destreza;             // Inteligencia de manejo (reduce la probabilidad de accidentarse)
    Color colorCuerpo;          // Color principal del chasis del monoplaza (tipo Color de Raylib)
    Color colorAcento;          // Color secundario o de detalles (alerones, números)
    char tipoMotor;             // Tipo de motor/rendimiento: 'H' (Híbrido), 'V' (V12), 'A' (Aero)
    float especVal1;            // Valor técnico específico 1 (ej: TurboBoost para Híbridos)
    float especVal2;            // Valor técnico específico 2 (ej: Eficiencia de recuperación)
};

// Vector estático que actúa como base de datos local "hardcoded" de los pilotos del juego.
static const std::vector<DatoPiloto> CATALOGO_PILOTOS = {
    {"Max Verstappen",    "Red Bull Racing",  342, 28.0f, 42.0f, 0.94f, 0.97f,
     {30,60,180,255}, {255,220,0,255},   'H', 1.35f, 0.92f},
    {"Lewis Hamilton",    "Mercedes",         338, 26.5f, 40.0f, 0.92f, 0.96f,
     {0,210,190,255}, {100,100,100,255}, 'H', 1.28f, 0.90f},
    {"Charles Leclerc",   "Ferrari",          336, 27.0f, 41.0f, 0.91f, 0.93f,
     {200,20,20,255},  {255,255,255,255},'H', 1.25f, 0.88f},
    {"Fernando Alonso",   "Aston Martin",     330, 25.0f, 39.0f, 0.89f, 0.95f,
     {0,100,60,255},   {255,180,0,255},  'H', 1.20f, 0.87f},
    {"Carlos Sainz",      "Ferrari",          334, 26.0f, 40.5f, 0.90f, 0.91f,
     {200,20,20,255},  {255,255,0,255},  'H', 1.22f, 0.86f},
    {"Lando Norris",      "McLaren",          335, 27.5f, 41.5f, 0.92f, 0.92f,
     {255,140,0,255},  {0,0,0,255},      'H', 1.24f, 0.89f},
    {"George Russell",    "Mercedes",         333, 26.0f, 40.0f, 0.91f, 0.90f,
     {0,210,190,255},  {50,50,50,255},   'H', 1.23f, 0.88f},
    {"Sergio Perez",      "Red Bull Racing",  338, 25.5f, 40.0f, 0.91f, 0.88f,
     {30,60,180,255},  {255,220,0,255},  'H', 1.30f, 0.91f},
};

// ═══════════════════════════════════════════════════════════
//  CLASE CONFIG (Manejador de archivos de entrada y salida)
// ═══════════════════════════════════════════════════════════
class Config {
public:
    // Traduce el nombre del clima (texto plano) a un multiplicador de fricción física.
    // Menor factor significa asfalto resbaladizo y más derrapes en las curvas.
    static float ClimaTofactor(const std::string& c) {
        if (c == "lluvia")   return 0.72f; // Pierde un 28% de agarre
        if (c == "tormenta") return 0.52f; // Pierde casi la mitad del agarre total
        if (c == "nublado")  return 0.88f; // Ligera baja de adherencia por temperatura de pista
        return 1.00f;                      // Soleado (agarre al 100%)
    }

    // Convierte el texto leído de la configuración en un enumerador fuertemente tipado (Enum).
    static Clima ClimaEnum(const std::string& c) {
        if (c == "lluvia")   return Clima::LLUVIA;
        if (c == "tormenta") return Clima::TORMENTA;
        if (c == "nublado")  return Clima::NUBLADO;
        return Clima::SOLEADO;
    }

    // Crea un archivo de texto por defecto si el simulador no encuentra ninguno al arrancar.
    static void CrearEjemplo(const std::string& ruta) {
        std::ofstream f(ruta); // Abre flujo de escritura en disco
        f << "# Configuracion Simulador F1\n"
          << "pista=monaco\n"
          << "clima=soleado\n"
          << "vueltas=5\n"
          << "num_autos=6\n"
          << "# piloto_idx (0-7 del catalogo maestro)\n"
          << "piloto=0\n"  // Verstappen (Asignado automáticamente al jugador)
          << "piloto=1\n"  // Hamilton
          << "piloto=2\n"  // Leclerc
          << "piloto=3\n"  // Alonso
          << "piloto=4\n"  // Sainz
          << "piloto=5\n"; // Norris
        f.close();         // Cierra y guarda el archivo de texto
    }

    // Lee el archivo de configuración externa y construye toda la estructura de la carrera
    static ConfigCarrera Leer(const std::string& ruta) {
        ConfigCarrera cfg;
        // 1. Valores por defecto preventivos por si el archivo está corrupto o incompleto
        cfg.vueltas     = 5;
        cfg.numAutos    = 4;
        cfg.clima       = Clima::SOLEADO;
        cfg.factorClima = 1.0f;
        cfg.nombreClima = "Soleado";
        cfg.pista       = PistaID::MONACO;

        std::ifstream f(ruta); // Intenta abrir el archivo en modo lectura
        if (!f.is_open()) {
            CrearEjemplo(ruta); // Si no existe, lo autogenera
            f.open(ruta);       // Y lo intenta abrir de nuevo
        }

        std::string linea;
        std::vector<int> idxPilotos; // Lista temporal para guardar los IDs de pilotos deseados

        // 2. Bucle de parseo (Procesamiento línea por línea)
        while (std::getline(f, linea)) {
            // Ignora líneas vacías o comentarios que inicien con '#'
            if (linea.empty() || linea[0] == '#') continue;

            auto sep = linea.find('='); // Encuentra la separación de clave y valor
            if (sep == std::string::npos) continue; // Si no hay '=', pasa de largo

            std::string clave = linea.substr(0, sep);  // Extrae texto de la izquierda (ej: "clima")
            std::string valor = linea.substr(sep+1);   // Extrae texto de la derecha (ej: "lluvia")

            // Evaluación de la clave encontrada
            if (clave == "pista") {
                if (valor == "silverstone") cfg.pista = PistaID::SILVERSTONE;
                else if (valor == "suzuka") cfg.pista = PistaID::SUZUKA;
                else                        cfg.pista = PistaID::MONACO;
            }
            else if (clave == "clima") {
                cfg.clima       = ClimaEnum(valor);
                cfg.factorClima = ClimaTofactor(valor);
                cfg.nombreClima = valor;
                cfg.nombreClima[0] = toupper(cfg.nombreClima[0]); // Capitaliza la primera letra (lluvia -> Lluvia)
            }
            else if (clave == "vueltas")   cfg.vueltas   = std::stoi(valor); // Convierte string a Entero
            else if (clave == "num_autos") cfg.numAutos  = std::stoi(valor);
            else if (clave == "piloto") {
                int idx = std::stoi(valor);
                // Validación para evitar desbordamientos de memoria fuera del catálogo (0 a 7)
                if (idx >= 0 && idx < (int)CATALOGO_PILOTOS.size())
                    idxPilotos.push_back(idx);
            }
        }
        f.close();

        // 3. CONSTRUCCIÓN DINÁMICA DE LA GRILLA DE AUTOS
        cfg.autos.clear(); // Limpia la lista del objeto carrera antes de rellenar
        for (int i = 0; i < (int)idxPilotos.size() && i < cfg.numAutos; i++) {
            const auto& dp = CATALOGO_PILOTOS[idxPilotos[i]]; // Acceso rápido al catálogo estático
            Auto a{}; // Instancia un auto completamente limpio en memoria vacía
            
            // Copia de propiedades base desde el catálogo
            a.piloto          = dp.nombre;
            a.equipo          = dp.equipo;
            a.inicial         = dp.nombre[0]; // Guarda la inicial del piloto (para el HUD/Mini-mapa)
            a.colorCuerpo     = dp.colorCuerpo;
            a.colorAcento     = dp.colorAcento;
            a.velocidadBase   = dp.velBase;
            a.velocidadActual = 0.0f; // Todos inician detenidos
            a.aceleracion     = dp.aceleracion;
            a.frenado         = dp.frenado;
            a.agarre          = dp.agarre;
            a.destreza        = dp.destreza;
            
            // Inicialización de orientaciones en el espacio 3D (Ángulos de Euler)
            a.yaw = a.roll = a.pitch = 0.0f;
            
            // Parrilla de salida escalonada: Cada coche inicia retrasado 8 metros respecto al anterior
            a.distanciaRecorrida = (float)(i * 8); 
            
            // Contadores de tiempos e hitos de carrera
            a.vuelta          = 0;
            a.tiempoVuelta    = 0.0f;
            a.mejorVuelta     = 0.0f;
            a.tiempoTotal     = 0.0f;
            a.posicion        = i + 1; // Posición deportiva inicial (1º, 2º, 3º...)
            a.accidentado     = false;
            a.turnosParado    = 0;
            
            // Asignación de Roles e Inteligencia Artificial
            a.esJugador       = (i == 0); // El primer auto listado en el archivo siempre será el Humano
            a.nivelIA         = std::min(3, 1 + i/2); // Escala la dificultad de la IA de forma progresiva
            a.tipoMotor       = dp.tipoMotor;
            a.derrapeIntensidad = 0.0f;
            a.derrapeFade     = 0.0f;
            
            // Inyección de la Unión Técnica del Motor (Estructuras mutuamente excluyentes en memoria)
            if (dp.tipoMotor == 'H') {
                a.motor.hibrido.turboBoost   = dp.especVal1;
                a.motor.hibrido.recuperacion = dp.especVal2;
            } else if (dp.tipoMotor == 'V') {
                a.motor.v12.aspiracion = dp.especVal1;
                a.motor.v12.peso       = dp.especVal2;
            } else {
                a.motor.aero.downforce = dp.especVal1;
                a.motor.aero.drag      = dp.especVal2;
            }
            cfg.autos.push_back(a); // Agrega el auto configurado al vector final de la carrera
        }
        return cfg; // Devuelve el objeto completo listo para ser simulado
    }

    // Guarda el estado de la configuración a un archivo de texto en disco (Persistencia de datos)
    static void Guardar(const ConfigCarrera& cfg, const std::string& ruta) {
        std::ofstream f(ruta); // Abre en modo escritura (sobrescribe el archivo anterior)
        
        // Operadores ternarios para convertir los Enums internos de vuelta a texto legible
        const char* pNombre = cfg.pista == PistaID::MONACO ? "monaco" :
                              cfg.pista == PistaID::SILVERSTONE ? "silverstone" : "suzuka";
                              
        const char* cNombre = cfg.clima == Clima::LLUVIA ? "lluvia" :
                              cfg.clima == Clima::TORMENTA ? "tormenta" :
                              cfg.clima == Clima::NUBLADO ? "nublado" : "soleado";
                              
        // Escritura estructurada de variables globales de entorno
        f << "# Configuracion Simulador F1\n"
          << "pista=" << pNombre << "\n"
          << "clima=" << cNombre << "\n"
          << "vueltas=" << cfg.vueltas << "\n"
          << "num_autos=" << cfg.numAutos << "\n";
          
        // Bucle para buscar y guardar los IDs correspondientes de los pilotos activos
        for (auto& a : cfg.autos) {
            for (int k = 0; k < (int)CATALOGO_PILOTOS.size(); k++) {
                if (CATALOGO_PILOTOS[k].nombre == a.piloto) {
                    f << "piloto=" << k << "\n"; // Escribe el índice del piloto encontrado en el catálogo
                    break; // Corta el bucle interno y avanza al siguiente auto
                }
            }
        }
        f.close(); // Salva y cierra el archivo de manera limpia
    }
};
