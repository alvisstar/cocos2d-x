/****************************************************************************
Copyright (c) 2010-2013 cocos2d-x.org
Copyright (c) Microsoft Open Technologies, Inc.

http://www.cocos2d-x.org

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/

#include "ShaderCaching.h"
#include "sha1.h"
#include "platform/winrt/CCWinRTUtils.h"

NS_CC_BEGIN

#if USE_SHADER_CACHING
bool cacheCompiledShader(GLuint program, const std::string& type, const std::string& sha)
{
    std::string cacheFile = sha;
    createMappedCacheFile(type, cacheFile, ".dat");

    ShaderFileHeader header;
    GLint writtenLength = 0;

    // First we find out the length of the program binary.
    glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH_OES, &(header.programLength));

    // Then we create a buffer of the correct length.
    std::vector<unsigned char> binaryData(header.programLength);

    // Then we retrieve the program binary.
    glGetProgramBinaryOES(program, header.programLength, &writtenLength, &(header.binaryFormat), (GLvoid*)binaryData.data());

    // Now we create a file for the buffer.
    HANDLE file = CreateFile2(std::wstring(cacheFile.begin(), cacheFile.end()).c_str(), GENERIC_WRITE, 0, CREATE_ALWAYS, NULL);

    bool returnValue = false;

    if (file != INVALID_HANDLE_VALUE)
    {
        // Firstly write metadata about the program binary to the file.
        DWORD bytesWritten = 0;
        BOOL success = WriteFile(file, &header, sizeof(header), &bytesWritten, NULL);

        if (success && (sizeof(header) == bytesWritten))
        {
            // If successful, then we write the binary data to the file too.
            success = WriteFile(file, binaryData.data(), static_cast<DWORD>(binaryData.size()), &bytesWritten, NULL);
            if (success && (bytesWritten == binaryData.size()))
            {
                returnValue = true;
            }
        }

        CloseHandle(file);
    }

    return returnValue;
}

bool loadCachedShader(GLuint program, const std::string& type, const std::string& sha)
{
    std::string cacheFile = sha;
    createMappedCacheFile(type, cacheFile, ".dat");

    // Load the requested file
    HANDLE file = CreateFile2(std::wstring(cacheFile.begin(), cacheFile.end()).c_str(), GENERIC_READ, 0, OPEN_EXISTING, NULL);

    bool returnValue = false;

    if (file != INVALID_HANDLE_VALUE)
    {
        DWORD bytesRead = 0;
        ShaderFileHeader header;
        std::vector<unsigned char> binaryData(0);

        // Firstly read the metadata about the program binary.
        BOOL success = ReadFile(file, &header, sizeof(header), &bytesRead, NULL);
        if (success && (sizeof(header) == bytesRead))
        {
            FILE_STANDARD_INFO info;
            if (GetFileInformationByHandleEx(file, FileStandardInfo, &info, sizeof(info)))
            {
                // If successful, read the binary data.
                binaryData.resize(static_cast<size_t>(info.EndOfFile.QuadPart - sizeof(header)));
                success = ReadFile(file, &(binaryData[0]), static_cast<DWORD>(binaryData.size()), &bytesRead, NULL);
                if (success && (header.programLength == bytesRead) && (binaryData.size() == bytesRead))
                {
                    // If successful, update the program's data. 
                    glProgramBinaryOES(program, GL_PROGRAM_BINARY_ANGLE, (const GLvoid*)&binaryData[0], static_cast<GLint>(binaryData.size()));

                    // Check the link status, which indicates whether glProgramBinaryOES() succeeded.
                    GLint linkStatus = 0;
                    glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
                    if (linkStatus != 0)
                    {
                        returnValue = true;
                    }
                    else
                    {
#ifdef _DEBUG
                        // Code to help debug programs failing to load.
                        GLint infoLogLength;
                        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLength);

                        if (infoLogLength > 0)
                        {
                            std::vector<GLchar> infoLog(infoLogLength);
                            glGetProgramInfoLog(program, infoLog.size(), NULL, &infoLog[0]);
                            OutputDebugStringA(&infoLog[0]);
                        }
#endif // _DEBUG
                    }
                }
            }
        }

        CloseHandle(file);
    }

    return returnValue;
}

std::string computeHash(const GLchar* vShaderByteArray, const GLchar* fShaderByteArray)
{
    SHA1Context sha;
    int err;
    err = SHA1Reset(&sha);
    std::string result = "";
    uint8_t hash[SHA1HashSize];
    char hashString[SHA1HashSize * 2 + 1];

    if (!err)
    {
        err = SHA1Input(&sha, (const unsigned char *)vShaderByteArray, strlen(vShaderByteArray));
    }
    if (!err)
    {
        err = SHA1Input(&sha, (const unsigned char *)fShaderByteArray, strlen(fShaderByteArray));
    }
    if (!err)
    {
        char* shader_version = (char*)glGetString(GL_SHADING_LANGUAGE_VERSION);
        err = SHA1Input(&sha, (const unsigned char *)shader_version, strlen(shader_version));
    }
    if (!err)
    {
        err = SHA1Result(&sha, hash);
    }
    if (!err)
    {
        SHA1ConvertMessageToString(hash, hashString);
        result = hashString;
    }

    return result;
}
#endif

NS_CC_END
