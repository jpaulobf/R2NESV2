if(NOT DEFINED RESOURCE_SOURCE OR NOT DEFINED RESOURCE_DESTINATION)
    message(FATAL_ERROR "RESOURCE_SOURCE and RESOURCE_DESTINATION are required")
endif()

# O INI pertence ao usuário. Atualiza os demais recursos e cria o padrão apenas
# quando ainda não existe uma configuração na pasta de destino.
file(COPY "${RESOURCE_SOURCE}/" DESTINATION "${RESOURCE_DESTINATION}"
     PATTERN "config.ini" EXCLUDE)
if(NOT EXISTS "${RESOURCE_DESTINATION}/config.ini" AND EXISTS "${RESOURCE_SOURCE}/config.ini")
    file(COPY "${RESOURCE_SOURCE}/config.ini" DESTINATION "${RESOURCE_DESTINATION}")
endif()
