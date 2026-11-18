/*
  Заметки по тестам:

  1. Проверяет, что кнопка загрузки ROM отображается на экране
  2. Проверяет, что заголовок слота для картриджа отображается
  3. Проверяет, что блок с управлением с клавиатуры на месте
  4. Проверяет, что индикатор POWER виден
  5. Проверяет, что до загрузки ROM показывается заглушка
     "Game screen will appear here"

  6. Проверяет, что при загрузке .txt появляется ошибка
     о неправильном типе файла
  7. Проверяет, что при загрузке слишком маленького файла
     появляется ошибка о неправильном размере

  8. Проверяет, что кнопка Upload ROM изначально активна
  9. Проверяет, что у зоны перетаскивания правильный aria-label
  10. Проверяет, что input принимает только .gb файлы
  11. Проверяет, что стандартный input скрыт
  12. Проверяет наличие сообщения о приватности файлов

  13. Проверяет, что во время загрузки кнопка Upload ROM
      блокируется
  14. Проверяет, что клик по Upload ROM вызывает click()
      у скрытого input
  15. Проверяет, что во время загрузки появляется
      текст "Reading cartridge…"

  16. Проверяет, что файл с неправильной контрольной суммой
      отклоняется загрузчиком
  17. Проверяет, что после успешной загрузки ROM
      появляется кнопка Play
  18. Проверяет, что до загрузки ROM кнопка Play отсутствует
*/

import { render, screen, fireEvent } from '@testing-library/react'
import Station from './station'


// ============================================================
// Совместимость File API с Jest / jsdom
// ============================================================

// В некоторых версиях jsdom отсутствует File.arrayBuffer()
// Добавляем реализацию через FileReader
if (!File.prototype.arrayBuffer) {
  File.prototype.arrayBuffer = function (): Promise<ArrayBuffer> {
    return new Promise((resolve, reject) => {
      const reader = new FileReader()

      reader.onload = () => resolve(reader.result as ArrayBuffer)
      reader.onerror = () => reject(reader.error)

      reader.readAsArrayBuffer(this)
    })
  }
}


// ============================================================
// Генерация тестового Game Boy ROM
// ============================================================

// Создаём тестовый ROM размером 32 KB
// Можно указать неправильную контрольную сумму для тестов
function createTestRom(
  fileName = 'test-rom.gb',
  validChecksum = true
): File {

  const romData = new Uint8Array(32768)

  // Записываем название тестового картриджа
  const title = 'TEST ROM'

  for (let i = 0; i < title.length; i++) {
    romData[0x134 + i] = title.charCodeAt(i)
  }

  // Размер ROM: 32 KB
  romData[0x148] = 0

  // Рассчитываем контрольную сумму заголовка
  let checksum = 0

  for (let i = 0x134; i <= 0x14c; i++) {
    checksum = (checksum - romData[i] - 1) & 0xff
  }

  // Записываем правильную или намеренно неправильную checksum
  romData[0x14d] = validChecksum
    ? checksum
    : (checksum + 1) & 0xff

  // Создаём File, который можно передать в input
  return new File(
    [romData],
    fileName,
    { type: 'application/octet-stream' }
  )
}


// Создаём валидный ROM для успешной загрузки
const validRom = createTestRom()


describe('Station (GameBoy Emulator UI)', () => {

  // ========== ЛЁГКИЕ ТЕСТЫ (отображение) ==========

  // 1. Проверяем наличие кнопки загрузки ROM
  test('renders upload button', () => {
    render(<Station />)

    expect(
      screen.getByRole('button', { name: /Upload ROM/i })
    ).toBeInTheDocument()
  })

  // 2. Проверяем заголовок слота для картриджа
  test('renders cartridge slot title', () => {
    render(<Station />)

    expect(
      screen.getByText(/Cartridge slot/i)
    ).toBeInTheDocument()
  })

  // 3. Проверяем наличие блока управления клавиатурой
  test('renders keyboard controls section', () => {
    render(<Station />)

    expect(
      screen.getByText(/Keyboard controls/i)
    ).toBeInTheDocument()
  })

  // 4. Проверяем индикатор питания консоли
  test('renders power indicator', () => {
    render(<Station />)

    expect(
      screen.getByText(/POWER/i)
    ).toBeInTheDocument()
  })

  // 5. Проверяем заглушку до загрузки игры
  test('renders standby message before ROM is loaded', () => {
    render(<Station />)

    expect(
      screen.getByText(/Game screen will appear here/i)
    ).toBeInTheDocument()
  })


  // ========== ТЕСТЫ НА ОШИБКИ ==========

  // 6. Проверяем, что нельзя загрузить файл с неправильным расширением
  test('shows error for invalid file type', async () => {
    render(<Station />)

    const file = new File(
      ['dummy content'],
      'game.txt',
      { type: 'text/plain' }
    )

    const input = screen.getByLabelText(
      /Select a Game Boy ROM/i
    )

    fireEvent.change(input, {
      target: { files: [file] }
    })

    expect(
      await screen.findByText(/Invalid file type/i)
    ).toBeInTheDocument()
  })

  // 7. Проверяем, что нельзя загрузить слишком маленький ROM
  test('shows error for invalid ROM size', async () => {
    render(<Station />)

    const file = new File(
      ['short'],
      'game.gb',
      { type: 'application/octet-stream' }
    )

    const input = screen.getByLabelText(
      /Select a Game Boy ROM/i
    )

    fireEvent.change(input, {
      target: { files: [file] }
    })

    expect(
      await screen.findByText(/Invalid ROM size/i)
    ).toBeInTheDocument()
  })

  // 16. Проверяем, что ROM с неправильной контрольной суммой отклоняется
  test('shows error for invalid cartridge header', async () => {
    render(<Station />)

    // Создаём ROM размером 32 KB с неправильной checksum
    const invalidRom = createTestRom('invalid.gb', false)

    const input = screen.getByLabelText(
      /Select a Game Boy ROM/i
    )

    fireEvent.change(input, {
      target: { files: [invalidRom] }
    })

    expect(
      await screen.findByText(/Invalid cartridge header/i)
    ).toBeInTheDocument()
  })


  // ========== ТЕСТЫ НА ВЗАИМОДЕЙСТВИЕ ==========

  // 8. Проверяем, что кнопку Upload ROM можно нажать
  test('upload button is clickable', () => {
    render(<Station />)

    const button = screen.getByRole('button', {
      name: /Upload ROM/i
    })

    expect(button).toBeEnabled()
  })

  // 9. Проверяем aria-label зоны перетаскивания
  test('drop zone has correct aria-label', () => {
    render(<Station />)

    expect(
      screen.getByLabelText(/Choose or drop a Game Boy ROM/i)
    ).toBeInTheDocument()
  })

  // 10. Проверяем, что input принимает только .gb файлы
  test('file input has accept attribute for .gb files', () => {
    render(<Station />)

    const input = screen.getByLabelText(
      /Select a Game Boy ROM/i
    )

    expect(input).toHaveAttribute('accept', '.gb')
  })

  // 11. Проверяем, что стандартный input скрыт
  test('file input is hidden', () => {
    render(<Station />)

    const input = screen.getByLabelText(
      /Select a Game Boy ROM/i
    )

    expect(input).toHaveAttribute('hidden')
  })

  // 12. Проверяем сообщение о приватности файлов
  test('renders "Files stay on your device" message', () => {
    render(<Station />)

    expect(
      screen.getByText(/Files stay on your device/i)
    ).toBeInTheDocument()
  })

  // 14. Проверяем, что кнопка Upload вызывает click() у input
  test('upload button click triggers file input', () => {
    render(<Station />)

    const uploadButton = screen.getByRole('button', {
      name: /Upload ROM/i
    })

    const input = screen.getByLabelText(
      /Select a Game Boy ROM/i
    ) as HTMLInputElement

    // Следим за вызовом click() у скрытого input
    const clickSpy = jest.spyOn(input, 'click')

    fireEvent.click(uploadButton)

    expect(clickSpy).toHaveBeenCalled()

    clickSpy.mockRestore()
  })


  // ========== ТЕСТЫ ЗАГРУЗКИ ROM ==========

  // 13. Проверяем, что во время загрузки кнопка блокируется
  test('disables upload button while busy', async () => {
    render(<Station />)

    const input = screen.getByLabelText(
      /Select a Game Boy ROM/i
    )

    // Используем сгенерированный тестовый ROM
    fireEvent.change(input, {
      target: { files: [validRom] }
    })

    // Во время чтения текст кнопки меняется
    const uploadButton = await screen.findByRole('button', {
      name: /Processing ROM/i
    })

    expect(uploadButton).toBeDisabled()
  })

  // 15. Проверяем появление текста Reading cartridge…
  test('shows loading state while reading cartridge', async () => {
    render(<Station />)

    const input = screen.getByLabelText(
      /Select a Game Boy ROM/i
    )

    // Загружаем сгенерированный тестовый ROM
    fireEvent.change(input, {
      target: { files: [validRom] }
    })

    // Проверяем состояние загрузки
    expect(
      await screen.findByText(/Reading cartridge…/i)
    ).toBeInTheDocument()
  })

  // 17. Проверяем основной сценарий:
  // успешная загрузка ROM -> появляется кнопка Play
  test('shows Play button after successful ROM upload', async () => {
    render(<Station />)

    const input = screen.getByLabelText(
      /Select a Game Boy ROM/i
    )

    // Передаём сгенерированный тестовый ROM
    fireEvent.change(input, {
      target: { files: [validRom] }
    })

    // Ждём завершения чтения и появления кнопки Play
    expect(
      await screen.findByRole('button', {
        name: /^Play$/i
      })
    ).toBeInTheDocument()

    // Проверяем, что название картриджа отображается
    expect(
      screen.getByText('test-rom.gb')
    ).toBeInTheDocument()
  })

  // 18. Проверяем, что до загрузки ROM кнопки Play нет
  test('does not show Play button before ROM is loaded', () => {
    render(<Station />)

    expect(
      screen.queryByRole('button', {
        name: /^Play$/i
      })
    ).not.toBeInTheDocument()
  })

})