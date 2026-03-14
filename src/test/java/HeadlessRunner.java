import com.codingame.gameengine.runner.MultiplayerGameRunner;
import com.codingame.gameengine.runner.simulate.GameResult;

/**
 * Headless match runner for genetic training.
 * Runs a match between two bots and outputs scores to stdout.
 *
 * Usage:
 *   java HeadlessRunner "command1" "command2" [seed]
 *
 * Output format (stdout):
 *   SCORES score0 score1
 */
public class HeadlessRunner {
    public static void main(String[] args) {
        if (args.length < 2) {
            System.err.println("Usage: HeadlessRunner <bot1_command> <bot2_command> [seed]");
            System.exit(1);
        }

        String bot1 = args[0];
        String bot2 = args[1];
        Long seed = null;
        if (args.length >= 3) {
            try {
                seed = Long.parseLong(args[2]);
            } catch (NumberFormatException e) {
                System.err.println("Invalid seed: " + args[2]);
                System.exit(1);
            }
        }

        try {
            MultiplayerGameRunner runner = new MultiplayerGameRunner();
            if (seed != null) {
                runner.setSeed(seed);
            }
            runner.addAgent(bot1, "Player 1");
            runner.addAgent(bot2, "Player 2");

            GameResult result = runner.simulate();

            // Extract scores from the game result
            if (result.scores != null && result.scores.size() >= 2) {
                int score0 = result.scores.getOrDefault(0, -1);
                int score1 = result.scores.getOrDefault(1, -1);
                System.out.println("SCORES " + score0 + " " + score1);
            } else {
                System.out.println("SCORES -1 -1");
            }
        } catch (Exception e) {
            System.err.println("Match error: " + e.getMessage());
            e.printStackTrace(System.err);
            System.out.println("SCORES -1 -1");
        }
    }
}
