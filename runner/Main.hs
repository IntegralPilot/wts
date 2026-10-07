{-# LANGUAGE ForeignFunctionInterface #-}
{-# LANGUAGE ScopedTypeVariables #-}

module Main (main) where

import Control.Exception
import Control.Monad (forM, forM_, when)
import Foreign.C.Error (throwErrnoIfMinus1_)
import Foreign.C.String (CString, withCString)
import Foreign.C.Types
import System.Directory (makeAbsolute, removePathForcibly)
import System.Environment (getArgs, getEnvironment)
import System.Exit
import System.FilePath ((</>))
import System.IO
import System.Posix.Signals (sigKILL, signalProcessGroup)
import System.Posix.Temp (mkdtemp)
import System.Posix.Unistd (getSystemID, release)
import System.Process
import System.Timeout (timeout)

import Payload (payloads, tests)

foreign import ccall unsafe "wts_page_size"
  pageSize :: IO CLong

foreign import ccall unsafe "wts_arch_ok"
  archOK :: IO CInt

foreign import ccall unsafe "wts_extract"
  extract :: CUInt -> CString -> IO CInt


runTest :: FilePath -> FilePath -> [(String, String)] -> IO Bool
runTest launcher path environment = mask $ \restore -> do
  (_, _, _, child) <- createProcess (proc launcher [path])
    { std_in = NoStream
    , std_out = UseHandle stdout
    , std_err = UseHandle stdout
    , env = Just environment
    , create_group = True
    , close_fds = True
    }

  pid <- getPid child

  let cleanup = do
        forM_ pid $ \p ->
          signalProcessGroup sigKILL p
            `catch` (\(_ :: IOException) -> pure ())

        -- Bound reaping too: a kernel fault can leave a task in uninterruptible sleep.
        _ <- timeout 2000000 (waitForProcess child)
        pure ()

  code <- restore (timeout 30000000 (waitForProcess child))
    `finally` cleanup

  case code of
    Just ExitSuccess ->
      pure True

    Just (ExitFailure n) -> do
      putStrLn ("exit/signal: " ++ show n)
      pure False

    Nothing -> do
      putStrLn "timed out after 30 seconds"
      pure False


runSuite :: FilePath -> IO ()
runSuite scratch = do
  page <- pageSize
  architecture <- archOK
  kernel <- release <$> getSystemID

  putStrLn ("WTS: kernel=" ++ kernel ++ " runner-page-size=" ++ show page)

  when (architecture /= 1) $
    dieSetup "requires AArch64 Linux"

  when (null tests) $
    dieSetup "no embedded tests"

  absolute <- makeAbsolute scratch

  bracket (mkdtemp (absolute </> "wts-")) removePathForcibly $ \dir -> do
    forM_ (zip [0..] payloads) $ \(index, name) ->
      withCString (dir </> name) $ \path ->
        throwErrnoIfMinus1_ "extract payload" (extract index path)

    inherited <- getEnvironment

    let environment = ("WTS_TMPDIR", dir)
          : filter ((/= "WTS_TMPDIR") . fst) inherited

    results <- forM tests $ \name -> do
      putStrLn ("RUN  " ++ name)

      passed <- runTest (dir </> "launch") (dir </> name) environment

      putStrLn ((if passed then "PASS " else "FAIL ") ++ name)

      pure passed

    let passed = length (filter id results)

    putStrLn (show passed ++ "/" ++ show (length results) ++ " passed")

    when (passed /= length results) $
      exitWith (ExitFailure 1)


dieSetup :: String -> IO a
dieSetup message = do
  putStrLn ("ERROR: " ++ message)
  exitWith (ExitFailure 2)


main :: IO ()
main = do
  hSetBuffering stdout LineBuffering

  args <- getArgs

  let action = case args of
        [] ->
          runSuite "/var/tmp"

        ["--help"] ->
          putStrLn "Usage: wts [scratch-directory]\nDefault: /var/tmp."

        [path] | take 1 path /= "-" ->
          runSuite path

        _ ->
          dieSetup "usage: wts [scratch-directory]"

  action `catch` (\(e :: IOException) -> dieSetup (displayException e))
    `catch` (\(e :: AsyncException) -> case e of
      UserInterrupt -> do
        putStrLn "ERROR: interrupted"
        exitWith (ExitFailure 130)

      _ ->
        throwIO e)
